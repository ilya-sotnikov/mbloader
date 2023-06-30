#ifndef MODBUSLOADER_H
#define MODBUSLOADER_H

#include "modbuscustomclient.h"

#include <qmodbusdataunit.h>
#include <qserialport.h>
#include <qfile.h>

class QModbusClient;

class ModbusLoader : public QObject
{
    Q_OBJECT

public:
    explicit ModbusLoader(QObject *parent = nullptr);
    void program(const QString &fileName);
    [[nodiscard]] auto &getErrorString() const { return errorStr; }
    void setDeviceSettings(const ModbusCustomClient::Settings &settings)
    {
        modbusClient->setSettings(settings);
    };
    [[nodiscard]] bool connectDevice();

private:
    enum class State {
        Idle,
        EraseFlash,
        ProgramFlash,
        VerifyFlash,
        ResetDevice,
        Finished,
    };

    ModbusCustomClient *modbusClient{ nullptr };
    QFile fileFirmware;
    quint8 checksumFirmware{ 0 };
    QString errorStr;
    State state{ State::Idle };

    void executeStateMachine();

signals:
    void finished(bool success);
    void newMessageAvailable(const QString &msg);
};

#endif // MODBUSLOADER_H