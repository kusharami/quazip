#pragma once
/*
Copyright (C) 2005-2014 Sergey A. Tachenov

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

#include "quazip/quazip.h"

#include <QtTest>

class QIODevice;
class QTextCodec;
class QTemporaryDir;

#define QADD_COLUMN(type, name) QTest::addColumn<type>(#name);

extern const QFile::Permissions defaultRead;
extern const QFile::Permissions defaultWrite;
extern const QFile::Permissions defaultReadWrite;

extern QuaZipFileInfo::Attribute winFileSystemAttr();
extern QuaZipFileInfo::Attribute winFileArchivedAttr();
extern QFile::Permissions execPermissions();

extern QString tempZipPath(const QTemporaryDir &tempDir, int num = 0);
extern QString tempFilesPath(const QTemporaryDir &tempDir, int num = 0);

extern bool createTestFiles(const QStringList &fileNames, int size = -1,
    const QString &filesPath = "tmp");
extern bool createTestArchive(const QString &zipName,
    const QStringList &fileNames, const QString &filesPath = "tmp");
extern bool createTestArchive(const QString &zipName,
    const QStringList &fileNames, QTextCodec *codec,
    const QString &filesPath = "tmp");
extern bool createTestArchive(const QString &zipName,
    const QStringList &fileNames, QTextCodec *codec, const QString &password,
    const QString &filesPath);

extern bool createTestArchive(QIODevice *ioDevice, const QStringList &fileNames,
    QTextCodec *codec, const QString &filesPath = "tmp");
extern bool createTestArchive(QIODevice *ioDevice, const QStringList &fileNames,
    QTextCodec *codec, const QString &password, const QString &filesPath);

struct SaveDefaultZipOptions {
    QuaZip::Compatibility compatibility;
    QTextCodec *defaultFilePathCodec;
    QTextCodec *defaultCommentCodec;
    QTextCodec *defaultPasswordCodec;

    SaveDefaultZipOptions();
    ~SaveDefaultZipOptions();
};

Q_DECLARE_METATYPE(QuaZip::CaseSensitivity)
Q_DECLARE_METATYPE(QuaZip::Compatibility)
Q_DECLARE_METATYPE(QuaZipFileInfo::Attributes)
Q_DECLARE_METATYPE(QFile::Permissions)
Q_DECLARE_METATYPE(QDir::Filters)
