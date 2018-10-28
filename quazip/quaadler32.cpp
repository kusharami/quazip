/*
Copyright (C) 2010 Adam Walczak
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

#include "quaadler32.h"

#include "zlib.h"

QuaAdler32::QuaAdler32()
{
    reset();
}

QuaAdler32::QuaAdler32(quint32 value)
{
    setValue(value);
}

quint32 QuaAdler32::calculate(const QByteArray &data)
{
    return adler32(adler32(0L, Z_NULL, 0),
        reinterpret_cast<const Bytef *>(data.data()), data.size());
}

void QuaAdler32::reset()
{
    setValue(adler32(0L, Z_NULL, 0));
}

void QuaAdler32::update(const QByteArray &buf)
{
    setValue(adler32(
        value(), reinterpret_cast<const Bytef *>(buf.data()), buf.size()));
}
