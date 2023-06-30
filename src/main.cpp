#include "modbusloader.h"

#include <qcoreapplication.h>
#include <qcommandlineparser.h>
#include <qvariant.h>
#include <qtimer.h>
#include <QMetaEnum>

using namespace Qt::Literals::StringLiterals;

template<typename EnumType>
static bool validateEnum(EnumType enumValue);
static bool validateClientSettings(ModbusCustomClient::Settings &settings);

static bool validateClientSettings(ModbusCustomClient::Settings &settings)
{
    if (!validateEnum(settings.baudRate))
        return false;
    if (!validateEnum(settings.dataBits))
        return false;
    if (!validateEnum(settings.parity))
        return false;
    if (!validateEnum(settings.stopBits))
        return false;

    if (settings.name.isEmpty())
        return false;

    if (settings.serverAddress < 1 || settings.serverAddress > 255)
        return false;

    return true;
}

template<typename EnumType>
static bool validateEnum(EnumType enumValue)
{
    const auto metaEnum{ QMetaEnum::fromType<EnumType>() };
    for (int i{ 0 }; i < metaEnum.keyCount(); ++i) {
        if (enumValue == metaEnum.value(i))
            return true;
    }
    return false;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QTextStream cout(stdout);
    QTextStream cerr(stderr);

    QCommandLineParser parser;
    parser.setApplicationDescription(u"Modbus loader"_s);
    parser.addHelpOption();
    parser.addPositionalArgument(u"firmware"_s, u"A bin file to load."_s);

    parser.addOptions({ { u"n"_s, u"Port name."_s, u"port"_s },
                        { u"b"_s, u"Baud rate. (115200)"_s, u"baud rate"_s, u"115200"_s },
                        { u"d"_s, u"Data bits. (8)"_s, u"data bits"_s, u"8"_s },
                        { u"p"_s, u"Parity. (N)"_s, u"parity"_s, u"N"_s },
                        { u"s"_s, u"Stop bits. (1)"_s, u"stop bits"_s, u"1"_s },
                        { u"a"_s, u"Server address."_s, u"address"_s } });

    parser.process(app);

    if (!parser.isSet(u"n"_s)) {
        cerr << u"Specify port name\n"_s << Qt::endl;
        parser.showHelp(1);
    }
    if (!parser.isSet(u"a"_s)) {
        cerr << u"Specify server address\n"_s << Qt::endl;
        parser.showHelp(1);
    }
    auto positionalArguments{ parser.positionalArguments() };
    if (positionalArguments.isEmpty()) {
        cerr << u"Specify firmware file (bin)\n"_s << Qt::endl;
        parser.showHelp(1);
    } else if (positionalArguments.size() > 1) {
        cerr << u"Several firmware files specified\n"_s << Qt::endl;
        parser.showHelp(1);
    }

    auto firmwareFile{ positionalArguments.first() };

    if (!firmwareFile.endsWith(u".bin"_s)) {
        cerr << u"Firmware file must be in bin format\n"_s << Qt::endl;
        parser.showHelp(1);
    }

    QSerialPort::Parity parity;
    switch (parser.value(u"p"_s).at(0).unicode()) {
    case 'N':
        parity = QSerialPort::NoParity;
        break;
    case 'E':
        parity = QSerialPort::EvenParity;
        break;
    case 'O':
        parity = QSerialPort::OddParity;
        break;
    case 'M':
        parity = QSerialPort::MarkParity;
        break;
    case 'S':
        parity = QSerialPort::SpaceParity;
        break;
    default:
        parity = QSerialPort::NoParity;
        break;
    }

    ModbusCustomClient::Settings settings{
        parser.value(u"n"_s),
        static_cast<QSerialPort::BaudRate>(parser.value(u"b"_s).toInt()),
        static_cast<QSerialPort::DataBits>(parser.value(u"d"_s).toInt()),
        parity,
        static_cast<QSerialPort::StopBits>(parser.value(u"s"_s).toInt()),
        parser.value(u"a"_s).toInt(),
    };

    cout << u"Connection parameters:\n"_s << u"Port name: %1\n"_s.arg(settings.name)
         << u"Baud rate: %1\n"_s.arg(QVariant::fromValue(settings.baudRate).toString())
         << u"Data bits: %1\n"_s.arg(QVariant::fromValue(settings.dataBits).toString())
         << u"Parity: %1\n"_s.arg(QVariant::fromValue(settings.parity).toString())
         << u"Stop bits: %1\n"_s.arg(QVariant::fromValue(settings.stopBits).toString())
         << u"Server address: %1\n"_s.arg(settings.serverAddress) << Qt::endl;

    if (!validateClientSettings(settings)) {
        cerr << "Modbus client settings are incorrect" << Qt::endl;
        return 1;
    }

    ModbusLoader modbusLoader{ &app };

    modbusLoader.setDeviceSettings(settings);

    if (!modbusLoader.connectDevice()) {
        cerr << modbusLoader.getErrorString() << Qt::endl;
        return 1;
    }

    QObject::connect(&modbusLoader, &ModbusLoader::finished, [&](bool success) {
        if (!success)
            cerr << modbusLoader.getErrorString() << Qt::endl;
        QCoreApplication::exit(!success);
    });

    QObject::connect(&modbusLoader, &ModbusLoader::newMessageAvailable,
                     [&](const QString &msg) { cout << msg << Qt::endl; });

    QTimer::singleShot(0, &modbusLoader, [&]() { modbusLoader.program(firmwareFile); });

    return app.exec();
}