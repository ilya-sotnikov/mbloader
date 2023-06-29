#include "modbuscustomclient.h"

#include <qdebug.h>
#include <qvariant.h>

using namespace Qt::Literals::StringLiterals;

ModbusCustomClient::ModbusCustomClient(QObject *parent) : QModbusRtuSerialClient{ parent }
{
    QModbusResponse::registerDataSizeCalculator(
            QModbusPdu::FunctionCode{ customFunctionCode },
            [](const QModbusResponse &response) -> int { return response.dataSize(); });
}

void ModbusCustomClient::setSettings(Settings settings)
{
    setConnectionParameter(QModbusDevice::SerialPortNameParameter, settings.name);
    setConnectionParameter(QModbusDevice::SerialBaudRateParameter, settings.baudRate);
    setConnectionParameter(QModbusDevice::SerialDataBitsParameter, settings.dataBits);
    setConnectionParameter(QModbusDevice::SerialStopBitsParameter, settings.stopBits);
    setConnectionParameter(QModbusDevice::SerialParityParameter, settings.parity);
    setServerAddress(settings.serverAddress);

    setTimeout(5000);
    setNumberOfRetries(1);
}

void ModbusCustomClient::modbusCustomFunction(Subfunctions subfunction, QByteArray &&data)
{
    data.prepend(static_cast<quint8>(subfunction));
    QModbusRequest request{ QModbusPdu::FunctionCode{ customFunctionCode }, data };

    auto reply{ sendRawRequest(request, serverAddress) };
    if (reply) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, [this, reply]() {
                if (reply->error() == QModbusDevice::NoError) {
                    result = reply->result().values();
                    emit finished(true);
                } else if (reply->error() == QModbusDevice::ProtocolError) {
                    // otherwise runtime error
                    // load of value 4294967276, which is not a valid value for type 'FunctionCode'
                    auto fixedResponse{ reply->rawResult() };
                    fixedResponse.setFunctionCode(static_cast<QModbusPdu::FunctionCode>(
                            static_cast<int>(customFunctionCode) | QModbusPdu::ExceptionByte));
                    reply->setRawResult(fixedResponse);
                    errorStr = reply->errorString()
                            + u" Exception code: 0x%1"_s.arg(fixedResponse.exceptionCode(), -1, 16);
                    emit finished(false);
                } else {
                    errorStr = reply->errorString();
                    emit finished(false);
                }
                reply->deleteLater();
            });
        } else {
            reply->deleteLater();
            emit finished(false);
            errorStr = u"Broadcast is not supported"_s;
        }
    } else {
        errorStr = errorString();
        emit finished(false);
    }
}

bool ModbusCustomClient::processPrivateResponse(const QModbusResponse &response,
                                                QModbusDataUnit *data)
{
    bool success{ false };
    auto dataSize{ response.dataSize() };

    if (dataSize < 1)
        return false;

    auto dataArray{ response.data() };
    Subfunctions subfunction{ static_cast<std::underlying_type_t<Subfunctions>>(
            dataArray.front()) };

    switch (subfunction) {
    case Subfunctions::EraseFlash:
        Q_ASSERT(dataSize == 2);
        data->setValues(QList<quint16>{ static_cast<quint16>(dataArray.at(1)) });
        success = true;
        break;

    case Subfunctions::ProgramFlash:
        Q_ASSERT(dataSize == 2);
        data->setValues(QList<quint16>{ static_cast<quint16>(dataArray.at(1)) });
        success = true;
        break;

    case Subfunctions::GetChecksumFlash:
        Q_ASSERT(dataSize == 2);
        data->setValues(QList<quint16>{ static_cast<quint8>(dataArray.at(1)) });
        success = true;
        break;

    case Subfunctions::ResetDevice:
        Q_ASSERT(dataSize == 2);
        data->setValues(QList<quint16>{ static_cast<quint8>(dataArray.at(1)) });
        success = true;
        break;
    }

    return success;
}