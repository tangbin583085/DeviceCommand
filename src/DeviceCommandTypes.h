#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QtGlobal>

#include <chrono>
#include <optional>

namespace devicecommand {

/** 指令失败的原因，业务层可以直接按枚举值处理。 */
enum class CommandError : quint8
{
    None,
    TransportUnavailable,
    SendFailed,
    Timeout,
    MaxRetryExceeded,
    ResponseMismatch,
    DeviceRejected,
    Disconnected,
    Cancelled,
    ProtocolError,
    InvalidCommand
};

/** 当前指令所处的阶段。 */
enum class CommandState : quint8
{
    Idle,
    Sending,
    WaitingResponse,
    Retrying,
    Completed,
    Cancelled,
    Failed
};

/** 交给调度器执行的一条设备指令。 */
struct DeviceCommand
{
    /** 协议里的指令号，0 作为无效值保留。 */
    quint8 commandId = 0;
    /** 请求序号。传 0 时由调度器自动分配。 */
    quint16 sequence = 0;
    /** 具体协议要发送的业务数据。 */
    QByteArray payload;
    /** 每次发送后等待响应的时长。 */
    std::chrono::milliseconds timeout{2000};
    /** 首次发送失败后，最多再试几次。 */
    int maxRetryCount = 0;
    /** 方便日志和排查问题，不会发给设备。 */
    QString description;
};

/** Codec 从字节流中拆出的一条完整响应。 */
struct DeviceResponse
{
    /** 响应里的请求序号。 */
    quint16 sequence = 0;
    /** 响应里的指令号。 */
    quint8 commandId = 0;
    /** 设备返回的状态码，Demo 协议中 0 表示成功。 */
    quint8 status = 0;
    /** 设备返回的业务数据。 */
    QByteArray payload;
    /** 已通过校验的完整原始帧。 */
    QByteArray rawData;
};

/** 一条指令最终结束时返回给业务层的结果。 */
struct CommandResult
{
    /** 原始指令，里面包含最终使用的 sequence。 */
    DeviceCommand command;
    /** 匹配到成功响应时为 true。 */
    bool success = false;
    /** 便于程序判断的错误类型。 */
    CommandError error = CommandError::None;
    /** 给日志或界面使用的错误说明，不包含敏感设备数据。 */
    QString errorMessage;
    /** 如果确实收到了匹配响应，这里会保留响应内容。 */
    std::optional<DeviceResponse> response;
    /** 实际发起过的重试次数。 */
    int retryCount = 0;
    /** 从开始执行到结束一共花了多久。 */
    qint64 elapsedMilliseconds = 0;
};

}

Q_DECLARE_METATYPE(devicecommand::CommandError)
Q_DECLARE_METATYPE(devicecommand::CommandState)
Q_DECLARE_METATYPE(devicecommand::DeviceCommand)
Q_DECLARE_METATYPE(devicecommand::DeviceResponse)
Q_DECLARE_METATYPE(devicecommand::CommandResult)
