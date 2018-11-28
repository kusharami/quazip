#include "quazextrafield.h"

#include <QDataStream>
#include <QBuffer>

QuaZExtraField::Map QuaZExtraField::toMap(const QByteArray &data)
{
    return toMap(data.data(), data.length());
}

QuaZExtraField::Map QuaZExtraField::toMap(const void *data, int length)
{
    Q_ASSERT(length == 0 || data != nullptr);
    Map result;

    auto extra = reinterpret_cast<const char *>(data);

    auto bytes = QByteArray::fromRawData(extra, length);
    QBuffer buffer(&bytes);
    ResultCode code = toMap(result, &buffer, length);
    Q_ASSERT(code == OK);
    Q_UNUSED(code);
    return result;
}

QuaZExtraField::ResultCode QuaZExtraField::toMap(
    Map &targetMap, QIODevice *sourceDevice, int length)
{
    Q_ASSERT(sourceDevice);
    targetMap.clear();
    if (length == 0)
        return OK;

    if (!sourceDevice->isOpen()) {
        sourceDevice->open(QIODevice::ReadOnly);
    }

    if (!sourceDevice->isReadable()) {
        return ERR_DEVICE_WRITE_FAILED;
    }

    QDataStream stream(sourceDevice);
    stream.setByteOrder(QDataStream::LittleEndian);

    int count = 0;

    while (stream.status() == QDataStream::Ok) {
        quint16 key;
        stream >> key;
        if (stream.status() == QDataStream::ReadPastEnd) {
            break;
        }
        quint16 len;
        stream >> len;
        if (length > 0) {
            count += sizeof(key) + sizeof(len) + len;
            if (count > length) {
                break;
            }
        }

        QByteArray bytes(len, Qt::Uninitialized);
        int readLength = stream.readRawData(bytes.data(), len);
        if (readLength != len) {
            return ERR_CORRUPTED_DATA;
        }

        if (!targetMap.contains(key)) {
            targetMap[key] = bytes;
        }
    }

    if (stream.status() != QDataStream::ReadCorruptData) {
        if (length < 0 || count == length) {
            return OK;
        }
    }

    return ERR_CORRUPTED_DATA;
}

QByteArray QuaZExtraField::fromMap(
    const Map &map, ResultCode *resultCode, int maxSize)
{
    QBuffer buffer;
    ResultCode code = fromMap(&buffer, map, maxSize);
    buffer.close();

    if (resultCode) {
        *resultCode = code;
    }

    return buffer.data();
}

QuaZExtraField::ResultCode QuaZExtraField::fromMap(
    QIODevice *targetDevice, const Map &map, int maxSize)
{
    Q_ASSERT(targetDevice);

    auto extraMax = maxSize < 0 ? std::numeric_limits<quint16>::max() : maxSize;

    if (!targetDevice->isOpen()) {
        targetDevice->open(QIODevice::WriteOnly | QIODevice::Truncate);
    }

    if (!targetDevice->isWritable()) {
        return ERR_DEVICE_WRITE_FAILED;
    }

    QDataStream stream(targetDevice);
    stream.setByteOrder(QDataStream::LittleEndian);

    int resultLength = 0;

    for (auto it = map.begin();
         it != map.end() && stream.status() == QDataStream::Ok; ++it) {
        auto &key = it.key();
        auto &fieldData = it.value();

        int fieldDataLength = fieldData.length();
        Q_CONSTEXPR int maxFieldLength = std::numeric_limits<quint16>::max();

        if (fieldDataLength > maxFieldLength) {
            return ERR_FIELD_SIZE_LIMIT;
        }

        if (resultLength + 4 + fieldDataLength > extraMax) {
            return ERR_BUFFER_SIZE_LIMIT;
        }

        stream << key;
        stream << quint16(fieldDataLength);
        stream.writeRawData(fieldData.data(), fieldDataLength);
    }

    if (stream.status() == QDataStream::WriteFailed) {
        return ERR_DEVICE_WRITE_FAILED;
    }

    return OK;
}

QString QuaZExtraField::errorString(ResultCode code)
{
    switch (code) {
    case OK:
        break;
    case ERR_FIELD_SIZE_LIMIT:
        return QStringLiteral("Extra Field data is to big.");
    case ERR_BUFFER_SIZE_LIMIT:
        return QStringLiteral(
            "Buffer size to store Extra Fields is not enough.");
    case ERR_CORRUPTED_DATA:
        return QStringLiteral("Extra Field data is corrupted.");
    case ERR_DEVICE_READ_FAILED:
        return QStringLiteral("Unable to read Extra Field data.");
    case ERR_DEVICE_WRITE_FAILED:
        return QStringLiteral("Unable to write Extra Field data.");
    }

    return QString();
}
