#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

namespace devicecommand {

/**
 * 调度器使用的字节传输接口。
 *
 * 串口、TCP、BLE 或厂商 SDK 都可以包在这里。实现时不要阻塞线程，核心库也不会
 * 直接依赖这些具体通信模块。
 */
class IDeviceTransport : public QObject
{
    Q_OBJECT

public:
    explicit IDeviceTransport(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~IDeviceTransport() override = default;

    /** 当前连接能不能用。 */
    virtual bool isConnected() const = 0;

    /**
     * 把数据交给底层发送，调用过程不能阻塞。
     *
     * 返回值只表示底层是否接收了这批数据，不代表设备已经收到。
     */
    virtual bool sendData(const QByteArray &data) = 0;

signals:
    /** 底层收到新数据时发出。 */
    void dataReceived(const QByteArray &data);
    /** 连接已经断开。 */
    void disconnected();
    /** 发送或接收过程中出现了异步错误。 */
    void transportError(const QString &message);
};

}
