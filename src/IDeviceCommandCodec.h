#pragma once

#include "DeviceCommandTypes.h"

#include <QByteArray>
#include <QList>

namespace devicecommand {

/** 负责协议编码和拆包，调度器本身不关心帧格式。 */
class IDeviceCommandCodec
{
public:
    virtual ~IDeviceCommandCodec() = default;

    /** 把指令编码成一帧可以直接发送的数据。 */
    virtual QByteArray encode(const DeviceCommand &command) = 0;

    /** 喂入任意长度的字节块，返回目前已经拆出的完整响应。 */
    virtual QList<DeviceResponse> feedData(const QByteArray &data) = 0;

    /** 判断这条响应是不是当前指令要等的那一条。 */
    virtual bool matches(const DeviceCommand &command,
                         const DeviceResponse &response) const = 0;
};

}
