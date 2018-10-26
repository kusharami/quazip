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

#include "quazip_global.h"

class QString;
class QuaZUtils {
public:
    /// Returns max block size for zlib io operation
    template<typename T>
    static inline Q_DECL_CONSTEXPR T maxBlockSize()
    {
        return sizeof(T) < sizeof(qint64)
            ? std::numeric_limits<T>::max()
            : T(std::numeric_limits<qint64>::max());
    }

    /// Adjusts block size for minimal zlib io operation for count
    template<typename T>
    static inline void adjustBlockSize(T &blockSize, qint64 count)
    {
        if (count < qint64(blockSize)) {
            blockSize = T(count);
        }
    }

    static bool isAscii(const QString &text);
};
