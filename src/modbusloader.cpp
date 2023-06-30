#include "modbusloader.h"

using namespace Qt::Literals::StringLiterals;

ModbusLoader::ModbusLoader(QObject *parent) : modbusClient{ new ModbusCustomClient{ parent } }
{
    connect(modbusClient, &ModbusCustomClient::finished, this, [this](bool success) {
        if (!success) {
            errorStr = modbusClient->getErrorString();
            emit finished(false);
        } else {
            executeStateMachine();
        }
    });
}

bool ModbusLoader::connectDevice()
{
    if (!modbusClient->connectDevice()) {
        errorStr = u"Connect: "_s + modbusClient->errorString();
        return false;
    }

    return true;
}

void ModbusLoader::program(const QString &fileName)
{
    fileFirmware.setFileName(fileName);
    if (!fileFirmware.open(QIODeviceBase::ReadOnly)) {
        errorStr = fileFirmware.errorString();
        emit finished(false);
        return;
    }

    state = State::Idle;
    executeStateMachine();
}

void ModbusLoader::executeStateMachine()
{
    switch (state) {
    case State::Idle:
        errorStr.clear();
        checksumFirmware = 0;
        state = State::EraseFlash;
        break;

    case State::EraseFlash:
        if (modbusClient->getResult().front() == 0) {
            state = State::ProgramFlash;
            emit newMessageAvailable(u"Erased flash"_s);
        } else {
            errorStr = u"Erasing flash failed"_s;
            emit finished(false);
        }
        break;

    case State::ProgramFlash:
        if (modbusClient->getResult().front() != 0) {
            errorStr = u"Programming flash failed"_s;
            emit finished(false);
        } else if (fileFirmware.atEnd()) {
            emit newMessageAvailable(
                    u"Flash programmed (fw size = %1 bytes)"_s.arg(fileFirmware.size()));
            state = State::VerifyFlash;
        }
        break;

    case State::VerifyFlash:
        if (checksumFirmware == modbusClient->getResult().at(0)) {
            emit newMessageAvailable(
                    u"Flash verified (checksum = 0x%1)"_s.arg(checksumFirmware, 0, 16));
            state = State::ResetDevice;
        } else {
            errorStr = u"Verifying flash failed"_s;
            emit finished(false);
        }
        break;

    case State::ResetDevice:
        if (modbusClient->getResult().front() == 0) {
            state = State::Finished;
            emit newMessageAvailable(u"Reset device"_s);
        } else {
            errorStr = u"Resetting device failed"_s;
            emit finished(false);
        }
        break;

    case State::Finished:
        break;
    }

    switch (state) {
    case State::Idle:
        break;

    case State::EraseFlash:
        emit newMessageAvailable(u"Erasing flash..."_s);
        modbusClient->modbusCustomFunction(ModbusCustomClient::Subfunctions::EraseFlash);
        break;

    case State::ProgramFlash: {
        emit newMessageAvailable(u"Programming flash... (%1%)"_s.arg(
                qRound(100.0 * fileFirmware.pos() / fileFirmware.size())));

        // 256 - server address (1 byte) - CRC (2 bytes) - function code (1 byte) -
        // subfunction code (1 byte) - flash offset address (4 bytes) = 247
        // rounding down to 246
        char offsetTemp[4];
        auto pos{ static_cast<quint32>(fileFirmware.pos() & 0xFFFFFFFF) };
        for (int i{ 0 }; i < 4; ++i)
            offsetTemp[i] = (pos >> 8 * i) & 0xFF;

        char firmwareTemp[246];

        QByteArray byteArray;
        QDataStream dataStream{ &byteArray, QIODeviceBase::WriteOnly };

        auto readCount{ fileFirmware.read(firmwareTemp, sizeof(firmwareTemp)) };
        if (readCount == -1) {
            errorStr = fileFirmware.errorString();
            emit finished(false);
            return;
        } else if (readCount == 0) {
            return;
        }

        for (int i{ 0 }; i < readCount; ++i)
            checksumFirmware += firmwareTemp[i];

        dataStream.writeRawData(offsetTemp, 4);
        dataStream.writeRawData(firmwareTemp, readCount);

        modbusClient->modbusCustomFunction(ModbusCustomClient::Subfunctions::ProgramFlash,
                                           std::move(byteArray));
        break;
    }

    case State::VerifyFlash:
        emit newMessageAvailable(u"Verifying flash..."_s);
        modbusClient->modbusCustomFunction(ModbusCustomClient::Subfunctions::GetChecksumFlash);
        break;

    case State::ResetDevice:
        emit newMessageAvailable(u"Resetting flash..."_s);
        modbusClient->modbusCustomFunction(ModbusCustomClient::Subfunctions::ResetDevice);
        break;

    case State::Finished:
        emit finished(true);
        break;
    }
}
