#include "DemoCommandCodec.h"
#include "DeviceCommandDispatcher.h"
#include "MockDeviceTransport.h"

#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

#include <chrono>
#include <memory>
#include <utility>

using namespace std::chrono_literals;
using devicecommand::CommandResult;
using devicecommand::DeviceCommand;
using devicecommand::DeviceCommandDispatcher;
using devicecommand::DeviceResponse;
using devicecommand::sample::DemoCommandCodec;
using devicecommand::sample::MockDeviceTransport;

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);

    MockDeviceTransport transport;
    auto codec = std::make_unique<DemoCommandCodec>();
    DeviceCommandDispatcher dispatcher(&transport, std::move(codec));

    QObject::connect(
        &dispatcher,
        &DeviceCommandDispatcher::commandSucceeded,
        [](const CommandResult &result) {
            qInfo() << "指令执行成功，用时"
                    << result.elapsedMilliseconds << "ms";
            if (result.response) {
                qInfo() << "响应数据:" << result.response->payload.toHex();
            }
            QCoreApplication::quit();
        });

    QObject::connect(
        &dispatcher,
        &DeviceCommandDispatcher::commandFailed,
        [](const CommandResult &result) {
            qWarning() << "指令执行失败:" << result.errorMessage;
            QCoreApplication::exit(1);
        });

    DeviceCommand command;
    command.commandId = 0x01;
    command.payload = QByteArray::fromHex("010203");
    command.timeout = 2s;
    command.maxRetryCount = 2;
    command.description = QStringLiteral("读取设备信息");

    QString errorMessage;
    const auto sequence = dispatcher.enqueue(command, &errorMessage);
    if (!sequence) {
        qCritical() << "指令入队失败:" << errorMessage;
        return 1;
    }

    qInfo() << "指令已入队，sequence =" << *sequence;

    QTimer::singleShot(25, &transport, [&transport, sequence] {
        DeviceResponse response;
        response.sequence = *sequence;
        response.commandId = 0x01;
        response.status = 0;
        response.payload = QByteArray::fromHex("102030");
        transport.injectReceivedData(DemoCommandCodec::encodeResponse(response));
    });

    return application.exec();
}
