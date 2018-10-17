/*
Copyright (C) 2018 Alexandra Cherdantseva

This file is part of QuaZIP.

QuaZIP is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 2.1 of the License, or
(at your option) any later version.

QuaZIP is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with QuaZIP.  If not, see <http://www.gnu.org/licenses/>.

See COPYING file for the full LGPL text.

Original ZIP package is copyrighted by Gilles Vollant and contributors,
see quazip/(un)zip.h files for details. Basically it's the zlib license.
*/

#pragma once

#include "quaziodevice.h"

#include <QMap>
#include <ctime>

class QuaGzipDevicePrivate;

/// A class to compress/decompress Gzip file data.
/**
  This class can be used to compress any data in Gzip file format written to
  QIODevice or decompress it back.
  Compressing data sent over a QTcpSocket is a good example.
  */
class QUAZIP_EXPORT QuaGzipDevice : public QuaZIODevice {
    Q_OBJECT

public:
    /// @cond internal
    enum
    {
        GZIP_FLAG = 16
    };
    /// @endcond

    /// Constructor.
    /**
      \param parent The parent object, as per QObject logic.
      */
    explicit QuaGzipDevice(QObject *parent = nullptr);
    /// Constructor.
    /**
      \param io The QIODevice to read/write.
      \param parent The parent object, as per QObject logic.
      */
    explicit QuaGzipDevice(QIODevice *io, QObject *parent = nullptr);
    virtual ~QuaGzipDevice() override;

    /// Returns maximum length for stored file name
    static int maxFileNameLength();
    /// Returns maximum length for stored comment
    static int maxCommentLength();

    /// Returns true if Gz header acquired to access
    /// stored file name, comment, modification time and extra fields
    /// Always true in write mode.
    bool headerIsProcessed() const;

    /// Stored original file name in a compressed device.
    /// When reading, check headerIsProcessed() first.
    QByteArray originalFileName() const;
    /// Original file name to store in a compressed device
    /**
      \param fileName The file name in Latin1 encoding.
        Generates error if longer than maxFileNameLength()
     */
    void setOriginalFileName(const QByteArray &fileName);

    /// Stored comment in a compressed device.
    /// When reading, check headerIsProcessed() first.
    QByteArray comment() const;
    /// Comment to store in a compressed device
    /**
      \param text Commentary in Latin1 encoding.
        Generates error if longer than maxCommentLength()
     */
    void setComment(const QByteArray &text);

    /// Stored modification time in a compressed device.
    /// When reading, check headerIsProcessed() first.
    time_t modificationTime() const;
    /// Modification time to store in a compressed device
    /**
      \param time Time stamp in UTC.
        If zero, current time is set when opened for write.
     */
    void setModificationTime(time_t time);

    struct ExtraFieldKey {
        char key[2];

        inline ExtraFieldKey();
        inline ExtraFieldKey(std::initializer_list<char> init);
        inline ExtraFieldKey(const char *si);
        inline ExtraFieldKey(char si1, char si2);
        inline ExtraFieldKey(const ExtraFieldKey &other);

        inline ExtraFieldKey &operator=(const ExtraFieldKey &other);

        inline bool operator==(const ExtraFieldKey &other) const;
        inline bool operator!=(const ExtraFieldKey &other) const;
        inline bool operator<(const ExtraFieldKey &other) const;
        inline bool operator>(const ExtraFieldKey &other) const;
        inline bool operator<=(const ExtraFieldKey &other) const;
        inline bool operator>=(const ExtraFieldKey &other) const;

        inline quint16 value() const;
    };

    /// Maps field values with 2-characters key
    using ExtraFieldMap = QMap<ExtraFieldKey, QByteArray>;

    /// Stored extra fields in a compressed device.
    /// When reading, check headerIsProcessed() first.
    ExtraFieldMap extraFields() const;
    /// Extra fields to store in a compressed device
    /**
      \param map Map of fields and values.
        Generates error if extra fields is too big.
     */
    void setExtraFields(const ExtraFieldMap &map);

private:
    inline QuaGzipDevicePrivate *d() const;
};

QuaGzipDevice::ExtraFieldKey::ExtraFieldKey()
{
    key[0] = 0;
    key[1] = 0;
}

QuaGzipDevice::ExtraFieldKey::ExtraFieldKey(std::initializer_list<char> init)
{
    key[0] = (init.size() > 0) ? *init.begin() : char(0);
    key[1] = (init.size() > 1) ? *(init.begin() + 1) : char(0);
}

QuaGzipDevice::ExtraFieldKey::ExtraFieldKey(const char *si)
{
    key[0] = si ? si[0] : char(0);
    key[1] = (si[0] == 0) ? char(0) : si[1];
}

QuaGzipDevice::ExtraFieldKey::ExtraFieldKey(char si1, char si2)
{
    key[0] = si1;
    key[1] = si2;
}

QuaGzipDevice::ExtraFieldKey::ExtraFieldKey(const ExtraFieldKey &other)
{
    operator=(other);
}

QuaGzipDevice::ExtraFieldKey &QuaGzipDevice::ExtraFieldKey::operator=(
    const ExtraFieldKey &other)
{
    key[0] = other.key[0];
    key[1] = other.key[1];
    return *this;
}

bool QuaGzipDevice::ExtraFieldKey::operator==(const ExtraFieldKey &other) const
{
    return value() == other.value();
}

bool QuaGzipDevice::ExtraFieldKey::operator!=(const ExtraFieldKey &other) const
{
    return !operator==(other);
}

bool QuaGzipDevice::ExtraFieldKey::operator<(const ExtraFieldKey &other) const
{
    return value() < other.value();
}

bool QuaGzipDevice::ExtraFieldKey::operator>(const ExtraFieldKey &other) const
{
    return value() > other.value();
}

bool QuaGzipDevice::ExtraFieldKey::operator<=(const ExtraFieldKey &other) const
{
    return value() <= other.value();
}

bool QuaGzipDevice::ExtraFieldKey::operator>=(const ExtraFieldKey &other) const
{
    return value() >= other.value();
}

quint16 QuaGzipDevice::ExtraFieldKey::value() const
{
    return quint16((key[1] << 8) | key[0]);
}
