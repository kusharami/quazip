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

    char buf[4096];

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
