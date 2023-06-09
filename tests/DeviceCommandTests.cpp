#include "DemoCommandCodec.h"
#include "DeviceCommandDispatcher.h"
#include "IDeviceTransport.h"

#include <QSignalSpy>
#include <QTest>
#include <QList>

#include <memory>

using devicecommand::CommandError;
using devicecommand::CommandResult;
using devicecommand::DeviceCommand;
using devicecommand::DeviceCommandDispatcher;
using devicecommand::DeviceResponse;
using devicecommand::sample::DemoCommandCodec;

namespace {

class TestTransport final : public devicecommand::IDeviceTransport
{
public:
    explicit TestTransport(QObject *parent = nullptr)
        : IDeviceTransport(parent)
    {
    }

    bool isConnected() const override { return connected_; }

    bool sendData(const QByteArray &data) override
    {
        if (!connected_ || !sendSuccessful_) {
            return false;
        }

        sentData_.append(data);
        return true;
    }

    void inject(const QByteArray &data) { emit dataReceived(data); }

    void disconnectTransport()
    {
        connected_ = false;
        emit disconnected();
    }

    void setSendSuccessful(bool successful) { sendSuccessful_ = successful; }

    int sendCount() const { return sentData_.size(); }
    QByteArray sentDataAt(int index) const { return sentData_.at(index); }
    QByteArray lastSentData() const
    {
        return sentData_.isEmpty() ? QByteArray() : sentData_.last();
    }

private:
    bool connected_ = true;
    bool sendSuccessful_ = true;
    QList<QByteArray> sentData_;
};

DeviceCommand makeCommand(quint8 commandId,
                          int timeoutMilliseconds = 40,
                          int maxRetryCount = 0)
{
    DeviceCommand command;
    command.commandId = commandId;
    command.payload = QByteArray::fromHex("010203");
    command.timeout = std::chrono::milliseconds(timeoutMilliseconds);
    command.maxRetryCount = maxRetryCount;
    command.description = QStringLiteral("测试指令");
    return command;
}

QByteArray responseFrame(const DeviceCommand &command,
                         quint8 status = 0,
                         const QByteArray &payload = QByteArray("ok"))
{
    DeviceResponse response;
    response.sequence = command.sequence;
    response.commandId = command.commandId;
    response.status = status;
    response.payload = payload;
    return DemoCommandCodec::encodeResponse(response);
}

}

class DeviceCommandTests final : public QObject
{
    Q_OBJECT

private slots:
    void firstCommandStartsImmediatelyAndFifo()
    {
        TestTransport transport;
        DeviceCommandDispatcher dispatcher(&transport,
                                           std::make_unique<DemoCommandCodec>());
        QSignalSpy succeeded(&dispatcher, &DeviceCommandDispatcher::commandSucceeded);

        const DeviceCommand first = makeCommand(1);
        const DeviceCommand second = makeCommand(2);
        dispatcher.enqueue(first);
        dispatcher.enqueue(second);

        QCOMPARE(transport.sendCount(), 1);
        QCOMPARE(dispatcher.pendingCount(), 1);
        QCOMPARE(dispatcher.currentCommand()->commandId, quint8(1));

        const auto current = dispatcher.currentCommand();
        QVERIFY(current.has_value());
        transport.inject(responseFrame(*current));
        QTRY_COMPARE(succeeded.count(), 1);
        QCOMPARE(transport.sendCount(), 2);
        QCOMPARE(dispatcher.currentCommand()->commandId, quint8(2));

        const auto next = dispatcher.currentCommand();
        QVERIFY(next.has_value());
        transport.inject(responseFrame(*next));
        QTRY_COMPARE(succeeded.count(), 2);
        QVERIFY(!dispatcher.hasCurrentCommand());
    }

    void onlyOneCommandIsSentAtATime()
    {
        TestTransport transport;
        DeviceCommandDispatcher dispatcher(&transport,
                                           std::make_unique<DemoCommandCodec>());

        dispatcher.enqueue(makeCommand(1));
        dispatcher.enqueue(makeCommand(2));
        dispatcher.enqueue(makeCommand(3));

        QCOMPARE(transport.sendCount(), 1);
        QCOMPARE(dispatcher.pendingCount(), 2);
    }

    void responseMatchesSequenceAndCommandId()
    {
        TestTransport transport;
        DeviceCommandDispatcher dispatcher(&transport,
                                           std::make_unique<DemoCommandCodec>());
        QSignalSpy unmatched(&dispatcher, &DeviceCommandDispatcher::unmatchedResponse);
        QSignalSpy succeeded(&dispatcher, &DeviceCommandDispatcher::commandSucceeded);

        dispatcher.enqueue(makeCommand(1));
        const auto current = dispatcher.currentCommand();
        QVERIFY(current.has_value());

        DeviceResponse wrongSequence;
        wrongSequence.sequence = current->sequence + 1;
        wrongSequence.commandId = current->commandId;
        transport.inject(DemoCommandCodec::encodeResponse(wrongSequence));

        DeviceResponse wrongCommand;
        wrongCommand.sequence = current->sequence;
        wrongCommand.commandId = 2;
        transport.inject(DemoCommandCodec::encodeResponse(wrongCommand));

        QTRY_COMPARE(unmatched.count(), 2);
        QCOMPARE(succeeded.count(), 0);
        QVERIFY(dispatcher.hasCurrentCommand());

        transport.inject(responseFrame(*current));
        QTRY_COMPARE(succeeded.count(), 1);
    }

    void codecHandlesHalfPacketAndStickyPackets()
    {
        DemoCommandCodec codec;
        DeviceResponse response;
        response.sequence = 7;
        response.commandId = 1;
        response.payload = QByteArray::fromHex("1020");
        const QByteArray frame = DemoCommandCodec::encodeResponse(response);

        QVERIFY(codec.feedData(frame.left(3)).isEmpty());
        QCOMPARE(codec.feedData(frame.mid(3)).size(), 1);

        const QByteArray second = DemoCommandCodec::encodeResponse(response);
        QCOMPARE(codec.feedData(frame + second).size(), 2);
    }

    void codecSkipsInvalidHeaderAndChecksum()
    {
        DemoCommandCodec codec;
        DeviceResponse response;
        response.sequence = 8;
        response.commandId = 1;
        QByteArray invalid = DemoCommandCodec::encodeResponse(response);
        invalid[invalid.size() - 1] = static_cast<char>(
            static_cast<unsigned char>(invalid.constData()[invalid.size() - 1]) ^ 0xff);
        const QByteArray valid = DemoCommandCodec::encodeResponse(response);
        const QByteArray oversized = QByteArray::fromHex("55AA000101001001");

        QCOMPARE(codec.feedData(QByteArray::fromHex("000102")
                                + oversized + invalid + valid).size(), 1);
    }

    void invalidCommandIsRejected()
    {
        TestTransport transport;
        DeviceCommandDispatcher dispatcher(&transport,
                                           std::make_unique<DemoCommandCodec>());
        DeviceCommand command;
        QString errorMessage;

        QVERIFY(!dispatcher.enqueue(command, &errorMessage).has_value());
        QVERIFY(!errorMessage.isEmpty());
        QCOMPARE(transport.sendCount(), 0);
    }

    void timeoutProducesTimeoutError()
    {
        TestTransport transport;
        DeviceCommandDispatcher dispatcher(&transport,
                                           std::make_unique<DemoCommandCodec>());
        QSignalSpy failed(&dispatcher, &DeviceCommandDispatcher::commandFailed);

        dispatcher.enqueue(makeCommand(1, 20, 0));
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 500);
        const auto result = qvariant_cast<CommandResult>(failed.at(0).at(0));
        QCOMPARE(result.error, CommandError::Timeout);
    }

    void timeoutRetriesAndCanSucceed()
    {
        TestTransport transport;
        DeviceCommandDispatcher dispatcher(&transport,
                                           std::make_unique<DemoCommandCodec>());
        QSignalSpy retrying(&dispatcher, &DeviceCommandDispatcher::commandRetrying);
        QSignalSpy succeeded(&dispatcher, &DeviceCommandDispatcher::commandSucceeded);

        dispatcher.enqueue(makeCommand(1, 20, 1));
        QTRY_COMPARE_WITH_TIMEOUT(retrying.count(), 1, 500);
        QTRY_COMPARE_WITH_TIMEOUT(transport.sendCount(), 2, 700);
        QCOMPARE(transport.sentDataAt(0), transport.sentDataAt(1));

        const auto current = dispatcher.currentCommand();
        QVERIFY(current.has_value());
        transport.inject(responseFrame(*current));
        QTRY_COMPARE(succeeded.count(), 1);
        QCOMPARE(transport.sendCount(), 2);
    }

    void retryLimitIsEnforced()
    {
        TestTransport transport;
        DeviceCommandDispatcher dispatcher(&transport,
                                           std::make_unique<DemoCommandCodec>());
        QSignalSpy failed(&dispatcher, &DeviceCommandDispatcher::commandFailed);

        dispatcher.enqueue(makeCommand(1, 15, 1));
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 700);
        QCOMPARE(transport.sendCount(), 2);
        const auto result = qvariant_cast<CommandResult>(failed.at(0).at(0));
        QCOMPARE(result.error, CommandError::MaxRetryExceeded);
        QCOMPARE(result.retryCount, 1);
    }

    void successfulResponseStopsFurtherRetries()
    {
        TestTransport transport;
        DeviceCommandDispatcher dispatcher(&transport,
                                           std::make_unique<DemoCommandCodec>());
        QSignalSpy succeeded(&dispatcher, &DeviceCommandDispatcher::commandSucceeded);

        dispatcher.enqueue(makeCommand(1, 20, 2));
        const auto current = dispatcher.currentCommand();
        QVERIFY(current.has_value());
        transport.inject(responseFrame(*current));
        QTRY_COMPARE(succeeded.count(), 1);
        QTest::qWait(180);
        QCOMPARE(transport.sendCount(), 1);
        QCOMPARE(succeeded.count(), 1);
    }

    void sendFailureCanRetry()
    {
        TestTransport transport;
        transport.setSendSuccessful(false);
        DeviceCommandDispatcher dispatcher(&transport,
                                           std::make_unique<DemoCommandCodec>());
        QSignalSpy failed(&dispatcher, &DeviceCommandDispatcher::commandFailed);

        dispatcher.enqueue(makeCommand(1, 20, 0));
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 500);
        const auto result = qvariant_cast<CommandResult>(failed.at(0).at(0));
        QCOMPARE(result.error, CommandError::SendFailed);
        QCOMPARE(transport.sendCount(), 0);
    }

    void sendFailureRetriesThenSucceeds()
    {
        TestTransport transport;
        transport.setSendSuccessful(false);
        DeviceCommandDispatcher dispatcher(&transport,
                                           std::make_unique<DemoCommandCodec>());
        QSignalSpy succeeded(&dispatcher, &DeviceCommandDispatcher::commandSucceeded);

        QObject::connect(
            &dispatcher,
            &DeviceCommandDispatcher::commandRetrying,
            &transport,
            [&transport](const DeviceCommand &, int) {
                transport.setSendSuccessful(true);
            });

        dispatcher.enqueue(makeCommand(1, 30, 1));
        QTRY_COMPARE_WITH_TIMEOUT(transport.sendCount(), 1, 500);
        const auto current = dispatcher.currentCommand();
        QVERIFY(current.has_value());
        transport.inject(responseFrame(*current));
        QTRY_COMPARE(succeeded.count(), 1);
        const auto result = qvariant_cast<CommandResult>(succeeded.at(0).at(0));
        QCOMPARE(result.retryCount, 1);
    }

    void deviceRejectionFailsImmediately()
    {
        TestTransport transport;
        DeviceCommandDispatcher dispatcher(&transport,
                                           std::make_unique<DemoCommandCodec>());
        QSignalSpy failed(&dispatcher, &DeviceCommandDispatcher::commandFailed);

        dispatcher.enqueue(makeCommand(1, 100, 2));
        const auto current = dispatcher.currentCommand();
        QVERIFY(current.has_value());
        transport.inject(responseFrame(*current, 3));
        QTRY_COMPARE(failed.count(), 1);
        const auto result = qvariant_cast<CommandResult>(failed.at(0).at(0));
        QCOMPARE(result.error, CommandError::DeviceRejected);
        QVERIFY(result.response.has_value());
        QCOMPARE(result.response->status, quint8(3));
        QCOMPARE(transport.sendCount(), 1);
    }

    void cancelCurrentStartsNext()
    {
        TestTransport transport;
        DeviceCommandDispatcher dispatcher(&transport,
                                           std::make_unique<DemoCommandCodec>());
        QSignalSpy cancelled(&dispatcher, &DeviceCommandDispatcher::commandCancelled);

        dispatcher.enqueue(makeCommand(1));
        dispatcher.enqueue(makeCommand(2));
        dispatcher.cancelCurrent();

        QTRY_COMPARE(cancelled.count(), 1);
        QCOMPARE(transport.sendCount(), 2);
        QCOMPARE(dispatcher.currentCommand()->commandId, quint8(2));
    }

    void cancelAllAndClearPendingDoNotRetry()
    {
        TestTransport transport;
        DeviceCommandDispatcher dispatcher(&transport,
                                           std::make_unique<DemoCommandCodec>());
        QSignalSpy cancelled(&dispatcher, &DeviceCommandDispatcher::commandCancelled);

        dispatcher.enqueue(makeCommand(1, 100, 2));
        dispatcher.enqueue(makeCommand(2));
        dispatcher.enqueue(makeCommand(3));
        dispatcher.clearPending();
        QCOMPARE(cancelled.count(), 2);
        QCOMPARE(dispatcher.pendingCount(), 0);
        QVERIFY(dispatcher.hasCurrentCommand());

        dispatcher.cancelAll();
        QCOMPARE(cancelled.count(), 3);
        QVERIFY(!dispatcher.hasCurrentCommand());
        QTest::qWait(180);
        QCOMPARE(transport.sendCount(), 1);
    }

    void disconnectFailsCurrentAndPending()
    {
        TestTransport transport;
        DeviceCommandDispatcher dispatcher(&transport,
                                           std::make_unique<DemoCommandCodec>());
        QSignalSpy failed(&dispatcher, &DeviceCommandDispatcher::commandFailed);

        dispatcher.enqueue(makeCommand(1, 100, 2));
        dispatcher.enqueue(makeCommand(2));
        transport.disconnectTransport();
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 2, 500);
        QCOMPARE(dispatcher.pendingCount(), 0);
        QVERIFY(!dispatcher.hasCurrentCommand());
        QCOMPARE(qvariant_cast<CommandResult>(failed.at(0).at(0)).error,
                 CommandError::Disconnected);
    }

    void transportDestructionFailsCurrent()
    {
        auto transport = std::make_unique<TestTransport>();
        DeviceCommandDispatcher dispatcher(transport.get(),
                                           std::make_unique<DemoCommandCodec>());
        QSignalSpy failed(&dispatcher, &DeviceCommandDispatcher::commandFailed);

        dispatcher.enqueue(makeCommand(1, 100, 1));
        transport.reset();
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 500);
        QCOMPARE(qvariant_cast<CommandResult>(failed.at(0).at(0)).error,
                 CommandError::Disconnected);
    }

    void commandCompletesOnlyOnce()
    {
        TestTransport transport;
        DeviceCommandDispatcher dispatcher(&transport,
                                           std::make_unique<DemoCommandCodec>());
        QSignalSpy succeeded(&dispatcher, &DeviceCommandDispatcher::commandSucceeded);

        dispatcher.enqueue(makeCommand(1));
        const auto current = dispatcher.currentCommand();
        QVERIFY(current.has_value());
        const QByteArray frame = responseFrame(*current);
        transport.inject(frame);
        transport.inject(frame);
        QTRY_COMPARE(succeeded.count(), 1);
    }

    void queueSignalsAndSequenceAreStable()
    {
        TestTransport transport;
        DeviceCommandDispatcher dispatcher(&transport,
                                           std::make_unique<DemoCommandCodec>());
        QSignalSpy queued(&dispatcher, &DeviceCommandDispatcher::commandQueued);
        QSignalSpy queueChanged(&dispatcher, &DeviceCommandDispatcher::queueChanged);

        const auto first = dispatcher.enqueue(makeCommand(1));
        const auto second = dispatcher.enqueue(makeCommand(2));
        QVERIFY(first.has_value());
        QVERIFY(second.has_value());
        QVERIFY(*first != *second);
        QCOMPARE(queued.count(), 2);
        QCOMPARE(dispatcher.pendingCount(), 1);
        QVERIFY(queueChanged.count() >= 3);
    }

    void dispatcherDestructionStopsTimer()
    {
        TestTransport transport;
        auto dispatcher = std::make_unique<DeviceCommandDispatcher>(
            &transport,
            std::make_unique<DemoCommandCodec>());
        QSignalSpy failed(dispatcher.get(), &DeviceCommandDispatcher::commandFailed);
        dispatcher->enqueue(makeCommand(1, 15, 1));
        dispatcher.reset();
        QTest::qWait(180);
        QCOMPARE(failed.count(), 0);
    }
};

QTEST_MAIN(DeviceCommandTests)
#include "DeviceCommandTests.moc"
