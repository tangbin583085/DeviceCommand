#pragma once

#include "IDeviceCommandCodec.h"

namespace devicecommand::sample {

/** README 中虚构协议的一个简单实现。 */
class DemoCommandCodec final : public IDeviceCommandCodec
{
public:
    QByteArray encode(const DeviceCommand &command) override;
    QList<DeviceResponse> feedData(const QByteArray &data) override;
    bool matches(const DeviceCommand &command,
                 const DeviceResponse &response) const override;

    /** 给示例和测试构造一帧虚构协议响应。 */
    static QByteArray encodeResponse(const DeviceResponse &response);

private:
    QByteArray receiveBuffer_;
};

}
