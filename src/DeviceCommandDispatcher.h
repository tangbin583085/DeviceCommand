#pragma once

#include "DeviceCommandTypes.h"
#include "IDeviceTransport.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QQueue>
#include <QTimer>

#include <memory>
#include <optional>

namespace devicecommand {

class IDeviceCommandCodec;

/**
 * 按 FIFO 顺序执行设备指令，一次只处理一条。
 *
 * Transport 和 Codec 由调用方传入。公共方法应在 Dispatcher 所在线程调用。
 */
class DeviceCommandDispatcher : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(DeviceCommandDispatcher)

public:
    explicit DeviceCommandDispatcher(
        IDeviceTransport *transport,
        std::unique_ptr<IDeviceCommandCodec> codec,
        QObject *parent = nullptr);
    ~DeviceCommandDispatcher() override;

    /**
     * 把指令放入队列，并返回最终分配到的 sequence。
     *
     * sequence 传 0 时由调度器自动生成。
     */
    std::optional<quint16> enqueue(DeviceCommand command,
                                   QString *errorMessage = nullptr);

    /** 当前是否有正在执行的指令。 */
    bool hasCurrentCommand() const;

    /** 返回当前指令的副本；空闲时返回 std::nullopt。 */
    std::optional<DeviceCommand> currentCommand() const;

    /** 当前调度状态。 */
    CommandState currentState() const;

    /** 当前指令后面还有多少条排队指令。 */
    int pendingCount() const;

public slots:
    /** 取消当前指令，然后继续执行下一条排队指令。 */
    void cancelCurrent();

    /** 取消当前指令，并取消所有还在等待的指令。 */
    void cancelAll();

    /** 只清空等待队列，不影响当前指令。 */
    void clearPending();

signals:
    /** 指令通过校验并进入队列后发出。 */
    void commandQueued(const devicecommand::DeviceCommand &command);
    /** 指令开始成为当前正在处理的指令时发出。 */
    void commandStarted(const devicecommand::DeviceCommand &command);
    /** 一帧数据被 Transport 接收后发出。 */
    void commandSent(const devicecommand::DeviceCommand &command);
    /** 即将进行重试时发出，参数是本次重试序号。 */
    void commandRetrying(const devicecommand::DeviceCommand &command,
                         int retryCount);
    /** 指令成功结束时发出，每条指令只发一次。 */
    void commandSucceeded(const devicecommand::CommandResult &result);
    /** 指令失败结束时发出，每条指令只发一次。 */
    void commandFailed(const devicecommand::CommandResult &result);
    /** 指令被主动取消时发出，每条指令只发一次。 */
    void commandCancelled(const devicecommand::CommandResult &result);
    /** 等待队列数量发生变化时发出。 */
    void queueChanged(int pendingCount);
    /** 收到格式正确但不属于当前指令的响应时发出。 */
    void unmatchedResponse(const devicecommand::DeviceResponse &response);
    /** 指令校验或调度器本身出现问题时发出。 */
    void dispatcherError(const QString &message);

private slots:
    void onDataReceived(const QByteArray &data);
    void onDisconnected();
    void onTransportError(const QString &message);
    void onTimerTimeout();

private:
    struct ActiveCommand
    {
        DeviceCommand command;
        CommandState state = CommandState::Idle;
        int retryCount = 0;
        QElapsedTimer elapsed;
        bool finished = false;
    };

    enum class TimerMode : quint8
    {
        None,
        ResponseTimeout,
        RetryDelay
    };

    bool validateCommand(const DeviceCommand &command,
                         QString *errorMessage);
    bool sequenceInUse(quint16 sequence) const;
    quint16 assignSequence();
    void startNext();
    void sendCurrent();
    void scheduleRetry(CommandError cause, const QString &message);
    void completeCurrentSuccess(const DeviceResponse &response);
    void completeCurrentFailure(CommandError error,
                                const QString &message,
                                std::optional<DeviceResponse> response =
                                    std::nullopt);
    void completeCurrentCancellation();
    void completePending(CommandError error, const QString &message);
    CommandResult makeResult(bool success,
                             CommandError error,
                             const QString &message,
                             std::optional<DeviceResponse> response =
                                 std::nullopt) const;

    QPointer<IDeviceTransport> transport_;
    std::unique_ptr<IDeviceCommandCodec> codec_;
    QQueue<DeviceCommand> pendingCommands_;
    std::optional<ActiveCommand> current_;
    QTimer *timer_ = nullptr;
    TimerMode timerMode_ = TimerMode::None;
    quint16 nextSequence_ = 1;
    int retryIntervalMilliseconds_ = 100;
};

}
