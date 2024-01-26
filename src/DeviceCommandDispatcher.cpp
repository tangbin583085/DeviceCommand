#include "DeviceCommandDispatcher.h"

#include "IDeviceCommandCodec.h"
#include "IDeviceTransport.h"

#include <QtGlobal>

#include <algorithm>
#include <limits>
#include <utility>

namespace devicecommand {

DeviceCommandDispatcher::DeviceCommandDispatcher(
    IDeviceTransport *transport,
    std::unique_ptr<IDeviceCommandCodec> codec,
    QObject *parent)
    : QObject(parent)
    , transport_(transport)
    , codec_(std::move(codec))
    , timer_(new QTimer(this))
{
    timer_->setSingleShot(true);

    qRegisterMetaType<DeviceCommand>();
    qRegisterMetaType<DeviceResponse>();
    qRegisterMetaType<CommandResult>();

    connect(timer_, &QTimer::timeout,
            this, &DeviceCommandDispatcher::onTimerTimeout);

    if (transport_) {
        connect(transport_, &IDeviceTransport::dataReceived,
                this, &DeviceCommandDispatcher::onDataReceived,
                Qt::QueuedConnection);
        connect(transport_, &IDeviceTransport::disconnected,
                this, &DeviceCommandDispatcher::onDisconnected,
                Qt::QueuedConnection);
        connect(transport_, &IDeviceTransport::transportError,
                this, &DeviceCommandDispatcher::onTransportError,
                Qt::QueuedConnection);
        connect(transport_, &QObject::destroyed,
                this, [this] { onDisconnected(); },
                Qt::QueuedConnection);
    }
}

DeviceCommandDispatcher::~DeviceCommandDispatcher()
{
    timer_->stop();
    timerMode_ = TimerMode::None;
}

std::optional<quint16> DeviceCommandDispatcher::enqueue(
    DeviceCommand command,
    QString *errorMessage)
{
    if (!validateCommand(command, errorMessage)) {
        return std::nullopt;
    }

    if (command.sequence == 0) {
        command.sequence = assignSequence();
        if (command.sequence == 0) {
            const QString message = QStringLiteral("No sequence is available");
            if (errorMessage) {
                *errorMessage = message;
            }
            emit dispatcherError(message);
            return std::nullopt;
        }
    }

    pendingCommands_.enqueue(command);
    emit commandQueued(command);
    emit queueChanged(pendingCommands_.size());

    if (!current_) {
        startNext();
    }

    return command.sequence;
}

bool DeviceCommandDispatcher::hasCurrentCommand() const
{
    return current_.has_value();
}

std::optional<DeviceCommand> DeviceCommandDispatcher::currentCommand() const
{
    if (!current_) {
        return std::nullopt;
    }

    return current_->command;
}

CommandState DeviceCommandDispatcher::currentState() const
{
    return current_ ? current_->state : CommandState::Idle;
}

int DeviceCommandDispatcher::pendingCount() const
{
    return pendingCommands_.size();
}

void DeviceCommandDispatcher::cancelCurrent()
{
    if (!current_) {
        return;
    }

    completeCurrentCancellation();
    startNext();
}

void DeviceCommandDispatcher::cancelAll()
{
    timer_->stop();
    timerMode_ = TimerMode::None;

    if (current_) {
        completeCurrentCancellation();
    }

    completePending(CommandError::Cancelled,
                    QStringLiteral("Command cancelled"));
}

void DeviceCommandDispatcher::clearPending()
{
    completePending(CommandError::Cancelled,
                    QStringLiteral("Pending command cleared"));
}

void DeviceCommandDispatcher::onDataReceived(const QByteArray &data)
{
    if (!codec_) {
        emit dispatcherError(QStringLiteral("No command codec is configured"));
        return;
    }

    const QList<DeviceResponse> responses = codec_->feedData(data);
    for (const DeviceResponse &response : responses) {
        if (!current_) {
            emit unmatchedResponse(response);
            continue;
        }

        if (!codec_->matches(current_->command, response)) {
            emit unmatchedResponse(response);
            continue;
        }

        if (response.status == 0) {
            completeCurrentSuccess(response);
        } else {
            completeCurrentFailure(
                CommandError::DeviceRejected,
                QStringLiteral("Device rejected command with status %1")
                    .arg(response.status),
                response);
        }

        startNext();
    }
}

void DeviceCommandDispatcher::onDisconnected()
{
    timer_->stop();
    timerMode_ = TimerMode::None;

    if (current_) {
        completeCurrentFailure(CommandError::Disconnected,
                               QStringLiteral("Transport disconnected"));
    }

    completePending(CommandError::Disconnected,
                    QStringLiteral("Transport disconnected"));
}

void DeviceCommandDispatcher::onTransportError(const QString &message)
{
    if (current_) {
        scheduleRetry(CommandError::SendFailed, message);
    } else {
        emit dispatcherError(message);
    }
}

void DeviceCommandDispatcher::onTimerTimeout()
{
    const TimerMode mode = timerMode_;
    timerMode_ = TimerMode::None;

    if (!current_ || current_->finished) {
        return;
    }

    if (mode == TimerMode::RetryDelay) {
        sendCurrent();
        return;
    }

    if (mode != TimerMode::ResponseTimeout) {
        return;
    }

    if (current_->retryCount < current_->command.maxRetryCount) {
        ++current_->retryCount;
        current_->state = CommandState::Retrying;
        emit commandRetrying(current_->command, current_->retryCount);
        timerMode_ = TimerMode::RetryDelay;
        timer_->start(retryIntervalMilliseconds_);
        return;
    }

    const CommandError error = current_->command.maxRetryCount == 0
        ? CommandError::Timeout
        : CommandError::MaxRetryExceeded;
    completeCurrentFailure(error,
                           error == CommandError::Timeout
                               ? QStringLiteral("Command timed out")
                               : QStringLiteral("Maximum retry count exceeded"));
    startNext();
}

bool DeviceCommandDispatcher::validateCommand(
    const DeviceCommand &command,
    QString *errorMessage)
{
    QString message;

    if (command.commandId == 0) {
        message = QStringLiteral("commandId must not be zero");
    } else if (command.timeout.count() <= 0) {
        message = QStringLiteral("timeout must be greater than zero");
    } else if (command.timeout.count() > std::numeric_limits<int>::max()) {
        message = QStringLiteral("timeout is too large for QTimer");
    } else if (command.maxRetryCount < 0) {
        message = QStringLiteral("maxRetryCount must not be negative");
    } else if (command.sequence != 0 && sequenceInUse(command.sequence)) {
        message = QStringLiteral("sequence is already in use");
    }

    if (!message.isEmpty()) {
        if (errorMessage) {
            *errorMessage = message;
        }
        emit dispatcherError(message);
        return false;
    }

    return true;
}

quint16 DeviceCommandDispatcher::assignSequence()
{
    for (int attempt = 0; attempt < std::numeric_limits<quint16>::max(); ++attempt) {
        const quint16 assigned = nextSequence_;
        ++nextSequence_;
        if (nextSequence_ == 0) {
            nextSequence_ = 1;
        }
        if (!sequenceInUse(assigned)) {
            return assigned;
        }
    }

    return 0;
}

bool DeviceCommandDispatcher::sequenceInUse(quint16 sequence) const
{
    if (current_ && current_->command.sequence == sequence) {
        return true;
    }

    return std::any_of(pendingCommands_.cbegin(), pendingCommands_.cend(),
                       [sequence](const DeviceCommand &command) {
                           return command.sequence == sequence;
                       });
}

void DeviceCommandDispatcher::startNext()
{
    if (current_ || pendingCommands_.isEmpty()) {
        return;
    }

    ActiveCommand active;
    active.command = pendingCommands_.dequeue();
    active.state = CommandState::Sending;
    active.elapsed.start();
    current_ = std::move(active);

    emit queueChanged(pendingCommands_.size());
    emit commandStarted(current_->command);

    if (!current_ || current_->finished) {
        return;
    }

    sendCurrent();
}

void DeviceCommandDispatcher::sendCurrent()
{
    if (!current_ || current_->finished) {
        return;
    }

    if (!transport_ || !codec_) {
        completeCurrentFailure(CommandError::TransportUnavailable,
                               QStringLiteral("Transport or codec is unavailable"));
        startNext();
        return;
    }

    if (!transport_->isConnected()) {
        completeCurrentFailure(CommandError::TransportUnavailable,
                               QStringLiteral("Transport is not connected"));
        startNext();
        return;
    }

    const QByteArray frame = codec_->encode(current_->command);
    if (frame.isEmpty()) {
        completeCurrentFailure(CommandError::ProtocolError,
                               QStringLiteral("Codec returned an empty frame"));
        startNext();
        return;
    }

    current_->state = CommandState::WaitingResponse;
    timerMode_ = TimerMode::ResponseTimeout;
    timer_->start(static_cast<int>(current_->command.timeout.count()));

    if (!transport_->sendData(frame)) {
        timer_->stop();
        timerMode_ = TimerMode::None;
        scheduleRetry(CommandError::SendFailed,
                      QStringLiteral("Transport rejected outgoing data"));
        return;
    }

    if (current_ && !current_->finished) {
        emit commandSent(current_->command);
    }
}

void DeviceCommandDispatcher::scheduleRetry(CommandError cause,
                                             const QString &message)
{
    if (!current_ || current_->finished) {
        return;
    }

    timer_->stop();
    timerMode_ = TimerMode::None;

    if (current_->retryCount < current_->command.maxRetryCount) {
        ++current_->retryCount;
        current_->state = CommandState::Retrying;
        emit commandRetrying(current_->command, current_->retryCount);
        timerMode_ = TimerMode::RetryDelay;
        timer_->start(retryIntervalMilliseconds_);
        return;
    }

    completeCurrentFailure(cause, message);
    startNext();
}

void DeviceCommandDispatcher::completeCurrentSuccess(
    const DeviceResponse &response)
{
    if (!current_) {
        return;
    }

    timer_->stop();
    timerMode_ = TimerMode::None;
    if (current_->finished) {
        return;
    }
    current_->finished = true;
    current_->state = CommandState::Completed;
    emit commandSucceeded(makeResult(true,
                                     CommandError::None,
                                     QString(),
                                     response));
    current_.reset();
}

void DeviceCommandDispatcher::completeCurrentFailure(
    CommandError error,
    const QString &message,
    std::optional<DeviceResponse> response)
{
    if (!current_) {
        return;
    }

    timer_->stop();
    timerMode_ = TimerMode::None;
    if (current_->finished) {
        return;
    }
    current_->finished = true;
    current_->state = CommandState::Failed;
    emit commandFailed(makeResult(false, error, message, std::move(response)));
    current_.reset();
}

void DeviceCommandDispatcher::completeCurrentCancellation()
{
    if (!current_) {
        return;
    }

    timer_->stop();
    timerMode_ = TimerMode::None;
    if (current_->finished) {
        return;
    }
    current_->finished = true;
    current_->state = CommandState::Cancelled;
    emit commandCancelled(makeResult(false,
                                     CommandError::Cancelled,
                                     QStringLiteral("Command cancelled")));
    current_.reset();
}

void DeviceCommandDispatcher::completePending(CommandError error,
                                               const QString &message)
{
    while (!pendingCommands_.isEmpty()) {
        CommandResult result;
        result.command = pendingCommands_.dequeue();
        result.error = error;
        result.errorMessage = message;
        if (error == CommandError::Cancelled) {
            emit commandCancelled(result);
        } else {
            emit commandFailed(result);
        }
    }

    emit queueChanged(0);
}

CommandResult DeviceCommandDispatcher::makeResult(
    bool success,
    CommandError error,
    const QString &message,
    std::optional<DeviceResponse> response) const
{
    CommandResult result;
    if (current_) {
        result.command = current_->command;
    }
    result.success = success;
    result.error = error;
    result.errorMessage = message;
    result.response = std::move(response);

    if (current_) {
        result.retryCount = current_->retryCount;
        result.elapsedMilliseconds = current_->elapsed.isValid()
            ? current_->elapsed.elapsed()
            : 0;
    }

    return result;
}

}
