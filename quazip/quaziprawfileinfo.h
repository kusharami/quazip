#pragma once

#include <QByteArray>
#include <QDateTime>

#include <zlib.h>

struct QuaZipRawFileInfo {
    quint16 versionMadeBy;
    quint16 versionNeeded;
    quint16 flags;
    quint16 compressionMethod;
    quint16 internalAttributes;
    qint32 externalAttributes;
    quint32 crc;
    int diskNumber;
    qint64 compressedSize;
    qint64 uncompressedSize;
    QDateTime modificationTime;
    QByteArray fileName;
    QByteArray localExtra;
    QByteArray centralExtra;
    QByteArray comment;

    inline QuaZipRawFileInfo()
        : versionMadeBy(10)
        , versionNeeded(10)
        , flags(0)
        , compressionMethod(Z_DEFLATED)
        , internalAttributes(0)
        , externalAttributes(0)
        , crc(0)
        , diskNumber(0)
        , compressedSize(0)
        , uncompressedSize(0)
    {
    }
};
