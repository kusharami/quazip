#include "quachecksum32.h"

#include <QIODevice>

QuaChecksum32::QuaChecksum32()
    : mValue(0)
{
}

QuaChecksum32::~QuaChecksum32() {}

bool QuaChecksum32::update(const QByteArray &ba, int size)
{
    if (size < 0)
        size = ba.length();

    if (size == 0)
        return true;

    if (size > ba.length())
        return false;

    update(ba.data(), size_t(size));
    return true;
}

bool QuaChecksum32::update(QIODevice *io, qint64 size)
{
    Q_ASSERT(io);

    if (size == 0)
        return true;

    char buf[8192];

    while (size != 0) {
        qint64 readBytes = sizeof(buf);
        if (size > 0 && size < readBytes)
            readBytes = size;

        auto actualReadBytes = io->read(buf, readBytes);

        if (actualReadBytes < 0 || (size > 0 && readBytes != actualReadBytes))
            return false;

        update(buf, size_t(actualReadBytes));

        if (size > 0) {
            size -= actualReadBytes;
        }
    }

    return true;
}
