#ifndef QUACHECKSUM32_H
#define QUACHECKSUM32_H

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
/** \class QuaChecksum32 quachecksum32.h <quazip/quachecksum32.h>
 * This is an interface for 32 bit checksums.
 * Classes implementing this interface can calcunate a certin
 * checksum in a single step:
 * \code
 * QChecksum32 *crc32 = new QuaCrc32();
 * rasoult = crc32->calculate(data);
 * \endcode
 * or by streaming the data:
 * \code
 * QChecksum32 *crc32 = new QuaCrc32();
 * while(!fileA.atEnd())
 *     crc32->update(fileA.read(bufSize));
 * resoultA = crc32->value();
 * crc32->reset();
 * while(!fileB.atEnd())
 *     crc32->update(fileB.read(bufSize));
 * resoultB = crc32->value();
 * \endcode
 */
class QUAZIP_EXPORT QuaChecksum32 {
    Q_DISABLE_COPY(QuaChecksum32)
public:
    QuaChecksum32();
    virtual ~QuaChecksum32();
    ///Calculates the checksum for data.
    /** \a data source data
     * \return data checksum
     *
     * This function has no efect on the value returned by value().
     */
    virtual quint32 calculate(const QByteArray &data) = 0;

    ///Resets the calculation on a checksun for a stream.
    virtual void reset() = 0;

    ///Updates the calculated checksum for the stream
    /** \a buf next portion of data from the stream
     */
    virtual void update(const QByteArray &buf) = 0;

    ///Value of the checksum calculated for the stream passed throw update().
    /** \return checksum
     */
    inline quint32 value() const;
    void setValue(quint32 value);

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
#endif //QUACHECKSUM32_H
