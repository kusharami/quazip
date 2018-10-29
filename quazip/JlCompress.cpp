/*
Copyright (C) 2010 Roberto Pompermaier
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

#include "JlCompress.h"

#include "quazutils.h"
#include "quazipdir.h"

#include "zip.h"
#include "unzip.h"

#include <QSaveFile>
#include <QTemporaryDir>

static bool copyData(QIODevice *inFile, QIODevice *outFile)
{
    while (true) {
        char buf[8192];
        qint64 readLen = inFile->read(buf, 8192);
        if (readLen < 0)
            return false;
        if (readLen == 0)
            break;
        if (outFile->write(buf, readLen) != readLen)
            return false;
    }
    return true;
}

static QString makeTempFilePath(const QTemporaryDir &tempDir)
{
    return QDir(tempDir.path()).filePath("quazip.temp");
}

bool JlCompress::compressFile(QuaZip *zip, QString fileName, QString fileDest)
{
    // zip: oggetto dove aggiungere il file
    // fileName: nome del file reale
    // fileDest: nome del file all'interno del file compresso

    // Controllo l'apertura dello zip
    if (!zip)
        return false;
    if (zip->getMode() != QuaZip::mdCreate &&
        zip->getMode() != QuaZip::mdAppend && zip->getMode() != QuaZip::mdAdd)
        return false;

    QScopedPointer<QFile> inFile;

    QFileInfo fileInfo(fileName);
    if (!fileInfo.isSymLink()) {
        // Apro il file originale
        inFile.reset(new QFile(fileName));
        if (!inFile->open(QIODevice::ReadOnly))
            return false;
    }

    // Apro il file risulato
    QuaZipFile outFile(zip);
    outFile.setFileInfo(QuaZipFileInfo::fromFile(fileName, fileDest));

    if (!outFile.open(QIODevice::WriteOnly))
        return false;

    if (inFile) {
        // Copio i dati
        if (!copyData(inFile.data(), &outFile) ||
            outFile.getZipError() != UNZ_OK) {
            return false;
        }
    }

    // Chiudo i file
    outFile.close();
    return outFile.getZipError() != UNZ_OK;
}

bool JlCompress::compressSubDir(QuaZip *zip, QString dir, QString origDir,
    bool recursive, QDir::Filters filters)
{
    // zip: oggetto dove aggiungere il file
    // dir: cartella reale corrente
    // origDir: cartella reale originale
    // (path(dir)-path(origDir)) = path interno all'oggetto zip

    // Controllo l'apertura dello zip
    if (!zip)
        return false;
    if (zip->getMode() != QuaZip::mdCreate &&
        zip->getMode() != QuaZip::mdAppend && zip->getMode() != QuaZip::mdAdd)
        return false;

    // Controllo la cartella
    QDir directory(dir);
    if (!directory.exists())
        return false;

    if (origDir.isEmpty())
        origDir = dir;

    QDir origDirectory(origDir);
    if (directory.absolutePath() != origDirectory.absolutePath()) {
        QuaZipFile dirZipFile(zip);
        dirZipFile.setFileInfo(QuaZipFileInfo::fromDir(
            directory, origDirectory.relativeFilePath(dir)));
        if (!dirZipFile.open(QIODevice::WriteOnly)) {
            return false;
        }
        dirZipFile.close();
    }

    // Se comprimo anche le sotto cartelle
    if (recursive) {
        // Per ogni sotto cartella
        auto filter = QDir::AllDirs | QDir::NoDotAndDotDot | filters;
        filter.setFlag(QDir::NoSymLinks, 0 == (filters & QDir::NoSymLinks));

        QFileInfoList files = directory.entryInfoList(filter);
        for (const auto &fileInfo : files) {
            // Comprimo la sotto cartella
            if (!compressSubDir(zip, fileInfo.absoluteFilePath(), origDir,
                    recursive, filters))
                return false;
        }
    }

    auto zipFilePath = QFileInfo(zip->zipFilePath()).absoluteFilePath();

    // Per ogni file nella cartella
    QFileInfoList files = directory.entryInfoList(QDir::Files | filters);
    for (const auto &fileInfo : files) {
        auto absFilePath = fileInfo.absoluteFilePath();
        // Se non e un file o e il file compresso che sto creando
        if ((!fileInfo.isFile() && !fileInfo.isSymLink()) ||
            absFilePath == zipFilePath)
            continue;

        // Creo il nome relativo da usare all'interno del file compresso
        QString storePath = origDirectory.relativeFilePath(absFilePath);

        if (filters & QDir::NoSymLinks) {
            if (!fileInfo.exists()) {
                continue;
            }
            absFilePath = fileInfo.canonicalFilePath();
        }

        // Comprimo il file
        if (!compressFile(zip, absFilePath, storePath))
            return false;
    }

    return true;
}

bool JlCompress::extractSingleFile(
    QuaZip *zip, QString fileName, QString fileDest)
{
    // zip: oggetto dove aggiungere il file
    // filename: nome del file reale
    // fileincompress: nome del file all'interno del file compresso

    // Controllo l'apertura dello zip
    if (!zip)
        return false;
    if (zip->getMode() != QuaZip::mdUnzip)
        return false;

    // Apro il file compresso
    if (!fileName.isEmpty())
        zip->setCurrentFile(fileName);
    QuaZipFile inFile(zip);
    if (!inFile.open(QIODevice::ReadOnly) || inFile.getZipError() != UNZ_OK)
        return false;

    // Controllo esistenza cartella file risultato
    QDir curDir;
    if (fileDest.endsWith('/')) {
        if (!curDir.mkpath(fileDest)) {
            return false;
        }
    } else {
        if (!curDir.mkpath(QFileInfo(fileDest).absolutePath())) {
            return false;
        }
    }

    QuaZipFileInfo info;
    if (!zip->getCurrentFileInfo(info))
        return false;

    switch (info.entryType()) {
    case QuaZipFileInfo::SymLink: {
        inFile.close();

        QuaZipDir dir(zip, info.path());
        bool isDir = false;
        auto &target = info.symLinkTarget();
        isDir = dir.cd(target);

        if (!QuaZUtils::createSymLink(fileDest, target, isDir))
            return false;

        break;
    }

    case QuaZipFileInfo::File: {
        // Apro il file risultato
        QSaveFile outFile(fileDest);
        if (!outFile.open(QIODevice::WriteOnly))
            return false;

        // Copio i dati
        if (!copyData(&inFile, &outFile))
            break;

        inFile.close();
        if (inFile.getZipError() == UNZ_OK)
            outFile.commit();
        break;
    }
    case QuaZipFileInfo::Directory:
        inFile.close();
        break;
    }

    if (inFile.getZipError() != UNZ_OK) {
        return false;
    }

    if (!info.applyAttributesTo(fileDest)) {
        qWarning("Unable to apply attributes for '%s'",
            fileDest.toLocal8Bit().data());
    }
    return true;
}

bool JlCompress::removeFile(QStringList listFile)
{
    bool ret = true;
    // Per ogni file
    for (int i = 0; i < listFile.count(); i++) {
        // Lo elimino
        ret = ret && QFile::remove(listFile.at(i));
    }
    return ret;
}

bool JlCompress::compressFile(QString zipArchive, QString file)
{
    if (!QDir().mkpath(QFileInfo(zipArchive).absolutePath()))
        return false;

    QSaveFile zipFile(zipArchive);
    // Creo lo zip
    QuaZip zip(&zipFile);
    zip.setAutoClose(false);
    if (!zip.open(QuaZip::mdCreate))
        return false;

    // Aggiungo il file
    if (!compressFile(&zip, file, QFileInfo(file).fileName())) {
        return false;
    }

    // Chiudo il file zip
    zip.close();
    if (zip.getZipError() == ZIP_OK) {
        zipFile.commit();
        return true;
    }

    return false;
}

bool JlCompress::compressFiles(QString zipArchive, QStringList files)
{
    if (!QDir().mkpath(QFileInfo(zipArchive).absolutePath()))
        return false;

    QSaveFile zipFile(zipArchive);
    // Creo lo zip
    QuaZip zip(&zipFile);
    zip.setAutoClose(false);
    if (!zip.open(QuaZip::mdCreate)) {
        return false;
    }

    // Comprimo i file
    QFileInfo info;
    for (const auto &file : files) {
        info.setFile(file);
        if ((!info.isFile() && !info.isSymLink()) ||
            !compressFile(&zip, file, info.fileName())) {
            return false;
        }
    }

    // Chiudo il file zip
    zip.close();
    if (zip.getZipError() == ZIP_OK) {
        zipFile.commit();
        return true;
    }

    return false;
}

bool JlCompress::compressDir(QString zipArchive, QString dir, bool recursive)
{
    return compressDir(zipArchive, dir, recursive, 0);
}

bool JlCompress::compressDir(
    QString zipArchive, QString dir, bool recursive, QDir::Filters filters)
{
    if (!QDir().mkpath(QFileInfo(zipArchive).absolutePath()))
        return false;

    QSaveFile zipFile(zipArchive);
    // Creo lo zip
    QuaZip zip(&zipFile);
    zip.setAutoClose(false);
    if (!zip.open(QuaZip::mdCreate)) {
        return false;
    }

    // Aggiungo i file e le sotto cartelle
    if (!compressSubDir(&zip, dir, dir, recursive, filters)) {
        return false;
    }

    // Chiudo il file zip
    zip.close();
    if (zip.getZipError() == ZIP_OK) {
        zipFile.commit();
        return true;
    }

    return false;
}

QString JlCompress::extractFile(
    QString zipArchive, QString fileName, QString fileDest)
{
    // Apro lo zip
    QuaZip zip(zipArchive);
    return extractFile(&zip, fileName, fileDest);
}

QString JlCompress::extractFile(QuaZip *zip, QString fileName, QString fileDest)
{
    if (!zip->open(QuaZip::mdUnzip)) {
        return QString();
    }

    QTemporaryDir tempDir;
    auto tempFilePath = makeTempFilePath(tempDir);

    if (!extractSingleFile(zip, fileName, tempFilePath)) {
        return QString();
    }

    // Chiudo il file zip
    zip->close();
    if (zip->getZipError() != 0) {
        return QString();
    }

    // Estraggo il file
    if (fileDest.isEmpty())
        fileDest = fileName;

    QFileInfo fileInfo(fileDest);
    fileInfo.setFile(fileInfo.absoluteFilePath());

    if (!QDir().mkpath(fileInfo.path()) ||
        !QFile::rename(tempFilePath, fileDest)) {
        return QString();
    }

    return fileDest;
}

QStringList JlCompress::extractFiles(
    QString zipArchive, QStringList files, QString dir)
{
    // Creo lo zip
    QuaZip zip(zipArchive);
    return extractFiles(&zip, files, dir);
}

QStringList JlCompress::extractFiles(
    QuaZip *zip, const QStringList &files, const QString &dir)
{
    if (!zip->open(QuaZip::mdUnzip)) {
        return QStringList();
    }

    QTemporaryDir tempDir;
    auto tempFilePath = makeTempFilePath(tempDir);

    // Estraggo i file
    QStringList extracted;
    for (auto &file : files) {
        if (!extractSingleFile(zip, file, tempFilePath)) {
            continue;
        }
        QString absPath = QDir(dir).absoluteFilePath(file);
        if (QDir().mkpath(QFileInfo(absPath).path()) &&
            QFile::rename(tempFilePath, absPath)) {
            extracted.append(absPath);
        }
    }

    // Chiudo il file zip
    zip->close();
    return extracted;
}

QStringList JlCompress::extractDir(QString zipArchive, QString dir)
{
    // Apro lo zip
    QuaZip zip(zipArchive);
    return extractDir(&zip, dir);
}

QStringList JlCompress::extractDir(QuaZip *zip, const QString &dir)
{
    if (!zip->open(QuaZip::mdUnzip)) {
        return QStringList();
    }
    QString cleanDir = QDir::cleanPath(dir);
    QDir directory(cleanDir);
    QString absCleanDir = directory.absolutePath();
    QStringList extracted;
    if (!zip->goToFirstFile()) {
        return QStringList();
    }

    QTemporaryDir tempDir;
    auto tempFilePath = makeTempFilePath(tempDir);

    do {
        QString name = zip->currentFilePath();
        QString absFilePath = directory.absoluteFilePath(name);
        QString absCleanPath = QDir::cleanPath(absFilePath);
        if (!absCleanPath.startsWith(absCleanDir + "/"))
            continue;
        if (!extractSingleFile(zip, QString(), tempFilePath)) {
            continue;
        }
        if (QDir().mkpath(QFileInfo(absFilePath).path()) &&
            QFile::rename(tempFilePath, absFilePath)) {
            extracted.append(absFilePath);
        }
    } while (zip->goToNextFile());

    // Chiudo il file zip
    zip->close();
    return extracted;
}

QStringList JlCompress::getFileList(QString fileCompressed)
{
    // Apro lo zip
    QuaZip zip(fileCompressed);
    return getFileList(&zip);
}

QStringList JlCompress::getFileList(QuaZip *zip)
{
    if (!zip->open(QuaZip::mdUnzip)) {
        delete zip;
        return QStringList();
    }

    // Estraggo i nomi dei file
    QStringList lst;
    QuaZipFileInfo info;
    for (bool more = zip->goToFirstFile(); more; more = zip->goToNextFile()) {
        if (!zip->getCurrentFileInfo(info)) {
            return QStringList();
        }
        lst << info.filePath();
    }

    // Chiudo il file zip
    zip->close();
    if (zip->getZipError() != 0) {
        return QStringList();
    }
    return lst;
}

QStringList JlCompress::extractDir(QIODevice *ioDevice, QString dir)
{
    QuaZip zip(ioDevice);
    return extractDir(&zip, dir);
}

QStringList JlCompress::getFileList(QIODevice *ioDevice)
{
    QuaZip zip(ioDevice);
    return getFileList(&zip);
}

QString JlCompress::extractFile(
    QIODevice *ioDevice, QString fileName, QString fileDest)
{
    QuaZip zip(ioDevice);
    return extractFile(&zip, fileName, fileDest);
}

QStringList JlCompress::extractFiles(
    QIODevice *ioDevice, QStringList files, QString dir)
{
    QuaZip zip(ioDevice);
    return extractFiles(&zip, files, dir);
}
