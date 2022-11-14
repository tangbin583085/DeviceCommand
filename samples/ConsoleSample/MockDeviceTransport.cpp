#include "MockDeviceTransport.h"

namespace devicecommand::sample {

MockDeviceTransport::MockDeviceTransport(QObject *parent)
    : IDeviceTransport(parent)
{
}

bool MockDeviceTransport::isConnected() const
{
    return connected_;
}

bool MockDeviceTransport::sendData(const QByteArray &data)
{
    if (!connected_ || !sendSuccessful_) {
        return false;
    }

    emit dataSent(data);
    return true;
}

void MockDeviceTransport::setConnected(bool connected)
{
    if (connected_ == connected) {
        return;
    }

    connected_ = connected;
    if (!connected_) {
        emit disconnected();
    }
}

void MockDeviceTransport::injectReceivedData(const QByteArray &data)
{
    emit dataReceived(data);
}

void MockDeviceTransport::setSendSuccessful(bool successful)
{
    sendSuccessful_ = successful;
}

}
