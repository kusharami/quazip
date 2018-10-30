/*
Copyright (C) 2005-2014 Sergey A. Tachenov
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

#include "qztest.h"
#include "testquazip.h"
#include "testquazipfile.h"
#include "testquachecksum32.h"
#include "testjlcompress.h"
#include "testquazipdir.h"
#include "testquagzipdevice.h"
#include "testquaziodevice.h"
#include "testquazipfileinfo.h"

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>

QString tempZipPath(const QTemporaryDir &tempDir, int num)
{
    return tempFilesPath(tempDir, num) + QStringLiteral(".zip");
}

QString tempFilesPath(const QTemporaryDir &tempDir, int num)
{
    return QDir(tempDir.path()).filePath(QStringLiteral("temp%1").arg(num));
}

bool createTestFiles(
    const QStringList &fileNames, int size, const QString &filesPath)
{
    QDir curDir;
    QDir dir(filesPath);
    for (const QString &fileName : fileNames) {
        QString filePath = dir.filePath(fileName);
        QDir testDir = QFileInfo(filePath).dir();
        if (!testDir.exists()) {
            if (!curDir.mkpath(testDir.path())) {
                qWarning("Couldn't mkpath %s",
                    testDir.path().toLocal8Bit().constData());
                return false;
            }
        }
        if (fileName.endsWith('/')) {
            if (!curDir.mkpath(filePath)) {
                qWarning(
                    "Couldn't mkpath %s", fileName.toLocal8Bit().constData());
                return false;
            }
        } else {
            QFile testFile(filePath);
            if (!testFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                qWarning(
                    "Couldn't create %s", fileName.toLocal8Bit().constData());
                return false;
            }
            if (size == -1) {
                QTextStream testStream(&testFile);
                testStream << "This is a test file named " << fileName << endl;
            } else {
                for (int i = 0; i < size; ++i) {
                    testFile.putChar(char('0' + i % 10));
                }
            }
        }
    }
    return true;
}

static bool createTestArchive(QuaZip &zip, const QString &zipName,
    const QStringList &fileNames, QTextCodec *codec, const QString &password,
    const QString &filesPath)
{
    if (codec != NULL) {
        zip.setFilePathCodec(codec);
        zip.setCompatibilityFlags(QuaZip::CustomCompatibility);
    }
    if (!zip.open(QuaZip::mdCreate)) {
        qWarning("Couldn't open %s", zipName.toLocal8Bit().constData());
        return false;
    }
    int i = 0;
    QDir dir(filesPath);
    for (const QString &fileName : fileNames) {
        QuaZipFile zipFile(&zip);
        QString filePath = dir.filePath(fileName);
        QFileInfo fileInfo(filePath);
        auto zipFileInfo =
            QuaZipFileInfo::fromFile(fileInfo, dir.relativeFilePath(filePath));
        zipFileInfo.setIsText(filePath.endsWith(".txt", Qt::CaseInsensitive));

        zipFile.setFileInfo(zipFileInfo);
        if (!password.isNull()) {
            QString pwd = password;
            zipFile.setPassword(&pwd);
        }
        if (!zipFile.open(QIODevice::WriteOnly)) {
            qWarning("Couldn't open %s in %s",
                fileName.toLocal8Bit().constData(),
                zipName.toLocal8Bit().constData());
            return false;
        }
        if (!fileInfo.isDir()) {
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly)) {
                qWarning(
                    "Couldn't open %s", filePath.toLocal8Bit().constData());
                return false;
            }
            while (true) {
                char buf[4096];
                qint64 l = file.read(buf, 4096);
                if (l < 0) {
                    qWarning(
                        "Couldn't read %s", filePath.toLocal8Bit().constData());
                    return false;
                }
                if (l == 0) {
                    break;
                }
                if (zipFile.write(buf, l) != l) {
                    qWarning("Couldn't write to %s in %s",
                        filePath.toLocal8Bit().constData(),
                        zipName.toLocal8Bit().constData());
                    return false;
                }
            }
        }
        ++i;
    }
    zip.setComment(QStringLiteral("This is the test archive"));
    zip.close();
    if (zipName.startsWith("<")) { // something like "<QIODevice pointer>"
        return true;
    }

    return QFileInfo::exists(zipName);
}

bool createTestArchive(const QString &zipName, const QStringList &fileNames,
    const QString &filesPath)
{
    return createTestArchive(zipName, fileNames, NULL, QString(), filesPath);
}

bool createTestArchive(QIODevice *ioDevice, const QStringList &fileNames,
    QTextCodec *codec, const QString &filesPath)
{
    QuaZip zip(ioDevice);
    return createTestArchive(
        zip, "<QIODevice pointer>", fileNames, codec, QString(), filesPath);
}

bool createTestArchive(const QString &zipName, const QStringList &fileNames,
    QTextCodec *codec, const QString &filesPath)
{
    QuaZip zip(zipName);
    return createTestArchive(
        zip, zipName, fileNames, codec, QString(), filesPath);
}

bool createTestArchive(const QString &zipName, const QStringList &fileNames,
    QTextCodec *codec, const QString &password, const QString &filesPath)
{
    QuaZip zip(zipName);
    return createTestArchive(
        zip, zipName, fileNames, codec, password, filesPath);
}

bool createTestArchive(QIODevice *ioDevice, const QStringList &fileNames,
    QTextCodec *codec, const QString &password, const QString &filesPath)
{
    QuaZip zip(ioDevice);
    return createTestArchive(
        zip, "<QIODevice pointer>", fileNames, codec, password, filesPath);
}

SaveDefaultZipOptions::SaveDefaultZipOptions()
{
    compatibility = QuaZip::defaultCompatibilityFlags();
    defaultFilePathCodec = QuaZip::defaultFilePathCodec();
    defaultCommentCodec = QuaZip::defaultCommentCodec();
    defaultPasswordCodec = QuaZip::defaultPasswordCodec();
}

SaveDefaultZipOptions::~SaveDefaultZipOptions()
{
    QuaZip::setDefaultCompatibilityFlags(compatibility);
    QuaZip::setDefaultFilePathCodec(defaultFilePathCodec);
    QuaZip::setDefaultCommentCodec(defaultCommentCodec);
    QuaZip::setDefaultPasswordCodec(defaultPasswordCodec);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    int err = 0;
    {
        TestQuaZip testQuaZip;
        err = qMax(err, QTest::qExec(&testQuaZip, app.arguments()));
    }
    {
        TestQuaZipFile testQuaZipFile;
        err = qMax(err, QTest::qExec(&testQuaZipFile, app.arguments()));
    }
    {
        TestQuaChecksum32 testQuaChecksum32;
        err = qMax(err, QTest::qExec(&testQuaChecksum32, app.arguments()));
    }
    {
        TestJlCompress testJlCompress;
        err = qMax(err, QTest::qExec(&testJlCompress, app.arguments()));
    }
    {
        TestQuaZipDir testQuaZipDir;
        err = qMax(err, QTest::qExec(&testQuaZipDir, app.arguments()));
    }
    {
        TestQuaZIODevice testQuaZIODevice;
        err = qMax(err, QTest::qExec(&testQuaZIODevice, app.arguments()));
    }
    {
        TestQuaGzipDevice testQuaGzipDevice;
        err = qMax(err, QTest::qExec(&testQuaGzipDevice, app.arguments()));
    }
    {
        TestQuaZipFileInfo testQuaZipFileInfo;
        err = qMax(err, QTest::qExec(&testQuaZipFileInfo, app.arguments()));
    }
    if (err == 0) {
        qInfo("All tests executed successfully");
    } else {
        qWarning("There were errors in some of the tests above.");
    }
    return err;
}
