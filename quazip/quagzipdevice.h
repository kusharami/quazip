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

class QUAZIP_EXPORT QuaGzipDevice : public QuaZIODevice {
    Q_OBJECT

public:
    enum
    {
        GZIP_FLAG = 16
    };

    explicit QuaGzipDevice(QObject *parent = nullptr);
    explicit QuaGzipDevice(QIODevice *io, QObject *parent = nullptr);
    virtual ~QuaGzipDevice() override;

    static int maxFileNameLength();
    static int maxCommentLength();

    bool headerIsProcessed() const;

    QByteArray storedFileName() const;
    void setStoredFileName(const QByteArray &fileName);

    QByteArray comment() const;
    void setComment(const QByteArray &text);

    time_t modificationTime() const;
    void setModificationTime(time_t time);

    struct ExtraFieldKey {
        char key[2];

        inline ExtraFieldKey();
        inline ExtraFieldKey(std::initializer_list<char> init);
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

    using ExtraFieldMap = QMap<ExtraFieldKey, QByteArray>;

    ExtraFieldMap extraFields() const;
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
    Q_ASSERT(init.size() == 2);
    key[0] = *init.begin();
    key[1] = *(init.begin() + 1);
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
