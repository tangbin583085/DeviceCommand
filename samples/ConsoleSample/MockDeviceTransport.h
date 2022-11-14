#pragma once

#include "IDeviceTransport.h"

namespace devicecommand::sample {

/** 内存里的测试 Transport，示例和单元测试都可以直接使用。 */
class MockDeviceTransport final : public IDeviceTransport
{
    Q_OBJECT

public:
    explicit MockDeviceTransport(QObject *parent = nullptr);

    bool isConnected() const override;
    bool sendData(const QByteArray &data) override;

    void setConnected(bool connected);
    void injectReceivedData(const QByteArray &data);
    void setSendSuccessful(bool successful);

signals:
    void dataSent(const QByteArray &data);

private:
    bool connected_ = true;
    bool sendSuccessful_ = true;
};

}
