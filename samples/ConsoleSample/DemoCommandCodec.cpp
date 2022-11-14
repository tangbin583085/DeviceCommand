#include "DemoCommandCodec.h"

#include <QtGlobal>

namespace devicecommand::sample {

namespace {

constexpr int kResponseHeaderSize = 8;
constexpr int kResponseOverhead = 9;
constexpr int kMaxPayloadLength = 4096;

quint8 byteAt(const QByteArray &data, int index)
{
    return static_cast<quint8>(static_cast<unsigned char>(data.at(index)));
}

quint8 checksumFor(const QByteArray &frame, int length)
{
    quint8 checksum = 0;
    for (int index = 0; index < length; ++index) {
        checksum ^= byteAt(frame, index);
    }
    return checksum;
}

void appendUInt16BigEndian(QByteArray *data, quint16 value)
{
    data->append(static_cast<char>((value >> 8) & 0xff));
    data->append(static_cast<char>(value & 0xff));
}

}

QByteArray DemoCommandCodec::encode(const DeviceCommand &command)
{
    if (command.commandId == 0
        || command.payload.size() > kMaxPayloadLength) {
        return {};
    }

    QByteArray frame;
    frame.reserve(8 + command.payload.size());
    frame.append(static_cast<char>(0xaa));
    frame.append(static_cast<char>(0x55));
    appendUInt16BigEndian(&frame, command.sequence);
    frame.append(static_cast<char>(command.commandId));
    appendUInt16BigEndian(&frame, static_cast<quint16>(command.payload.size()));
    frame.append(command.payload);
    frame.append(static_cast<char>(checksumFor(frame, frame.size())));
    return frame;
}

QList<DeviceResponse> DemoCommandCodec::feedData(const QByteArray &data)
{
    receiveBuffer_.append(data);

    QList<DeviceResponse> responses;
    const QByteArray header = QByteArray::fromHex("55AA");

    while (true) {
        const qsizetype headerIndex = receiveBuffer_.indexOf(header);
        if (headerIndex < 0) {
            if (!receiveBuffer_.isEmpty()
                && receiveBuffer_.back() == static_cast<char>(0x55)) {
                receiveBuffer_ = QByteArray(1, static_cast<char>(0x55));
            } else {
                receiveBuffer_.clear();
            }
            break;
        }

        if (headerIndex > 0) {
            receiveBuffer_.remove(0, headerIndex);
        }

        if (receiveBuffer_.size() < kResponseHeaderSize) {
            break;
        }

        const quint16 payloadLength =
            (static_cast<quint16>(byteAt(receiveBuffer_, 6)) << 8)
            | byteAt(receiveBuffer_, 7);
        if (payloadLength > kMaxPayloadLength) {
            receiveBuffer_.remove(0, 2);
            continue;
        }

        const int frameSize = kResponseOverhead + payloadLength;
        if (receiveBuffer_.size() < frameSize) {
            break;
        }

        const QByteArray frame = receiveBuffer_.left(frameSize);
        if (checksumFor(frame, frameSize - 1) != byteAt(frame, frameSize - 1)) {
            receiveBuffer_.remove(0, frameSize);
            continue;
        }

        DeviceResponse response;
        response.sequence =
            (static_cast<quint16>(byteAt(frame, 2)) << 8)
            | byteAt(frame, 3);
        response.commandId = byteAt(frame, 4);
        response.status = byteAt(frame, 5);
        response.payload = frame.mid(8, payloadLength);
        response.rawData = frame;
        responses.append(response);
        receiveBuffer_.remove(0, frameSize);
    }

    return responses;
}

bool DemoCommandCodec::matches(const DeviceCommand &command,
                               const DeviceResponse &response) const
{
    return command.sequence == response.sequence
        && command.commandId == response.commandId;
}

QByteArray DemoCommandCodec::encodeResponse(const DeviceResponse &response)
{
    if (response.commandId == 0 || response.payload.size() > kMaxPayloadLength) {
        return {};
    }

    QByteArray frame;
    frame.reserve(kResponseOverhead + response.payload.size());
    frame.append(static_cast<char>(0x55));
    frame.append(static_cast<char>(0xaa));
    appendUInt16BigEndian(&frame, response.sequence);
    frame.append(static_cast<char>(response.commandId));
    frame.append(static_cast<char>(response.status));
    appendUInt16BigEndian(&frame, static_cast<quint16>(response.payload.size()));
    frame.append(response.payload);
    frame.append(static_cast<char>(checksumFor(frame, frame.size())));
    return frame;
}

}
