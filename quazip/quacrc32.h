#pragma once

/*
Copyright (C) 2005-2014 Sergey A. Tachenov
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

///CRC32 checksum
/** \class QuaCrc32
* This class wraps the crc32 function with the QuaChecksum32 interface.
* See QuaChecksum32 for more info.
*/
class QUAZIP_EXPORT QuaCrc32 : public QuaChecksum32 {
public:
    QuaCrc32();
    QuaCrc32(quint32 value);
    QuaCrc32(const QuaCrc32 &other);

    virtual void reset() override;
    virtual void update(const void *data, size_t size) override;

    using QuaChecksum32::update;

    inline QuaCrc32 &operator=(const QuaCrc32 &other);
    inline bool operator==(const QuaCrc32 &other) const;
    inline bool operator!=(const QuaCrc32 &other) const;
};

QuaCrc32 &QuaCrc32::operator=(const QuaCrc32 &other)
{
    setValue(other.value());
    return *this;
}

bool QuaCrc32::operator==(const QuaCrc32 &other) const
{
    return value() == other.value();
}

bool QuaCrc32::operator!=(const QuaCrc32 &other) const
{
    return !operator==(other);
}
