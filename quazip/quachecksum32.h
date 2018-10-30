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

#include <QByteArray>
#include "quazip_global.h"

/// Checksum interface.
/** \class QuaChecksum32
 * This is an interface for 32 bit checksums.
 * You can calculate checksum like that:
 * \code
 * const char buf[] = "CONST TEXT";
 * QuaCrc32 crc32;
 * crc32.update(buf, sizeof(buf)-1);
 * crc32A = crc32.value();
 *
 * crc32.reset();
 * QByteArray test("Byte array data");
 * crc32.update(test);
 * crc32B = crc32.value();
 *
 * QFile fileA("file.txt");
 * fileA.open(QFile::ReadOnly);
 * crc32.update(fileA);
 * crc32C = crc32.value();
 *
 * crc32D = zChecksum<QuaCrc32>(test);
 * \endcode
 */
class QIODevice;
class QUAZIP_EXPORT QuaChecksum32 {
    Q_DISABLE_COPY(QuaChecksum32)
public:
    QuaChecksum32();
    virtual ~QuaChecksum32();

    ///Resets the calculation on a checksum.
    virtual void reset() = 0;

    ///Updates the calculated checksum for the data
    /**
     * @param data Should point to a valid data in memory of specivied \a size
     * @param size Size of data for checksum calculation
     */
    virtual void update(const void *data, size_t size) = 0;
    ///Updates the calculated checksum for the data in QIODevice
    /**
    * @param io IO device with data you want to calculate checksum for
    * @param size Count of bytes for checksum calculation, if negative
    * io->size() will be used
    *
    * @returns false if cannot read specified count of bytes from device,
    * true on success
    */
    bool update(QIODevice *io, qint64 size = -1);
    ///Updates the calculated checksum for the data
    /**
    * @param ba Byte array for checksum calculation
    * @param size Count of bytes for checksum calculation, if negative
    * ba.size() will be used
    *
    * @returns false if size > ba.size(), true on success
    */
    bool update(const QByteArray &ba, int size = -1);

    /// Returns value of the checksum calculated for the stream passed through update().
    inline quint32 value() const;
    /// Resets checksum \a value
    inline void setValue(quint32 value);

private:
    quint32 mValue;
};

quint32 QuaChecksum32::value() const
{
    return mValue;
}

void QuaChecksum32::setValue(quint32 value)
{
    mValue = value;
}

/// Calculates checksum for \a io device, if \a size is negative then,
/// io->size() is used
template<typename T>
inline quint32 zChecksum(QIODevice *io, qint64 size = -1)
{
    T calculator;
    calculator.update(io, size);
    return calculator.value();
}

/// Calculates checksum for a byte array, if \a size is negative then,
/// ba.size() is used
template<typename T>
inline quint32 zChecksum(const QByteArray &ba, int size = -1)
{
    T calculator;
    calculator.update(ba, size);
    return calculator.value();
}

/// Calculates checksum for \a data of specified \a size
template<typename T>
inline quint32 zChecksum(const void *data, size_t size)
{
    T calculator;
    calculator.update(data, size);
    return calculator.value();
}
