#pragma once
/*
Copyright (C) 2018 Alexandra Cherdantseva

This file is part of QuaZIP test suite.

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

#include <QObject>

class TestQuaZipFileInfo : public QObject {
    Q_OBJECT
public:
    explicit TestQuaZipFileInfo(QObject *parent = nullptr);
private slots:
    void testFromFile_data();
    void testFromFile();
    void testFromDir_data();
    void testFromDir();
    void testFromLink_data();
    void testFromLink();
    void testFromZipFile_data();
    void testFromZipFile();
};
