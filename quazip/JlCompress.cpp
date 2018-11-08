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
#include <QFileInfo>

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

bool JlCompress::compressFile(
    QuaZip *zip, const QString &fileName, const QString &fileDest)
{
    // zip: oggetto dove aggiungere il file
    // fileName: nome del file reale
    // fileDest: nome del file all'interno del file compresso

    // Controllo l'apertura dello zip
    if (!zip)
        return false;
    if (zip->openMode() != QuaZip::mdCreate &&
        zip->openMode() != QuaZip::mdAppend && zip->openMode() != QuaZip::mdAdd)
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
            outFile.zipError() != UNZ_OK) {
            return false;
        }
    }

    // Chiudo i file
    outFile.close();
    return outFile.zipError() == ZIP_OK;
}

bool JlCompress::compressSubDir(QuaZip *zip, const QString &dir,
    const QDir &origDir, bool recursive, QDir::Filters filters)
{
    // zip: oggetto dove aggiungere il file
    // dir: cartella reale corrente
    // origDir: cartella reale originale
    // (path(dir)-path(origDir)) = path interno all'oggetto zip

    // Controllo l'apertura dello zip
    if (!zip)
        return false;
    if (zip->openMode() != QuaZip::mdCreate &&
        zip->openMode() != QuaZip::mdAppend && zip->openMode() != QuaZip::mdAdd)
        return false;

    // Controllo la cartella
    QDir directory(dir);
    if (!directory.exists())
        return false;

    if (directory != origDir) {
        QuaZipFile dirZipFile(zip);
        dirZipFile.setFileInfo(
            QuaZipFileInfo::fromDir(directory, origDir.relativeFilePath(dir)));
        if (!dirZipFile.open(QIODevice::WriteOnly)) {
            return false;
        }
        dirZipFile.close();
        if (dirZipFile.zipError() != ZIP_OK)
            return false;
    }

    // Se comprimo anche le sotto cartelle
    if (recursive) {
        // Per ogni sotto cartella
        auto filter = QDir::Dirs | QDir::NoDotAndDotDot |
            (filters & ~(QDir::Files | QDir::AllDirs));
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
    QFileInfoList files = directory.entryInfoList(QDir::Files |
        (filters &
            ~(QDir::NoSymLinks | QDir::Drives | QDir::Dirs | QDir::AllDirs)));
    for (const auto &fileInfo : files) {
        auto absFilePath = fileInfo.absoluteFilePath();
        // Se non e un file o e il file compresso che sto creando
        if ((!fileInfo.isFile() && !fileInfo.isSymLink()) ||
            absFilePath == zipFilePath)
            continue;

        // Creo il nome relativo da usare all'interno del file compresso
        QString storePath = origDir.relativeFilePath(absFilePath);

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
    QuaZip *zip, const QString &fileName, const QString &fileDest)
{
    // zip: oggetto dove aggiungere il file
    // filename: nome del file reale
    // fileincompress: nome del file all'interno del file compresso

    // Controllo l'apertura dello zip
    if (!zip)
        return false;
    if (zip->openMode() != QuaZip::mdUnzip)
        return false;

    // Apro il file compresso
    if (!fileName.isEmpty())
        zip->setCurrentFile(fileName);

    QuaZipFileInfo info;
    if (!zip->getCurrentFileInfo(info))
        return false;

    QuaZipFile inFile(zip);
    if (!inFile.open(QIODevice::ReadOnly) || inFile.zipError() != UNZ_OK)
        return false;

    QFileInfo destInfo(fileDest);
    // Controllo esistenza cartella file risultato
    QDir curDir;
    if (fileDest.endsWith('/')) {
        if (!curDir.mkpath(fileDest)) {
            return false;
        }
    } else {
        if (!curDir.mkpath(destInfo.absolutePath())) {
            return false;
        }
    }

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
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;

        // Copio i dati
        if (!copyData(&inFile, &outFile))
            break;

        inFile.close();
        if (inFile.zipError() == UNZ_OK)
            outFile.commit();
        break;
    }
    case QuaZipFileInfo::Directory:
        inFile.close();
        break;
    }

    if (inFile.zipError() != UNZ_OK) {
        return false;
    }

    if (!info.applyAttributesTo(fileDest)) {
        qWarning("Unable to apply attributes for '%s'",
            fileDest.toLocal8Bit().data());
    }
    return true;
}

bool JlCompress::removeFile(const QStringList &listFile)
{
    bool ret = true;
    // Per ogni file
    for (int i = 0; i < listFile.count(); i++) {
        // Lo elimino
        ret = ret && QFile::remove(listFile.at(i));
    }
    return ret;
}

bool JlCompress::compressFile(
    const QString &zipArchive, const QString &file, const QString &targetDir)
{
    if (!QDir().mkpath(QFileInfo(zipArchive).absolutePath()))
        return false;

    QSaveFile zipFile(zipArchive);
    if (!zipFile.open(QFile::WriteOnly | QFile::Truncate))
        return false;
    // Creo lo zip
    QuaZip zip(&zipFile);
    if (!zip.open(QuaZip::mdCreate))
        return false;

    QDir dir(targetDir);

    QFileInfo fileInfo(file);

    if (!fileInfo.isFile())
        return false;

    // Aggiungo il file
    if (!compressFile(&zip, fileInfo.canonicalFilePath(),
            dir.filePath(fileInfo.fileName()))) {
        return false;
    }

    // Chiudo il file zip
    zip.close();
    if (zip.zipError() == ZIP_OK) {
        zipFile.commit();
        return true;
    }

    return false;
}

bool JlCompress::compressFiles(const QString &zipArchive,
    const QStringList &files, const QString &targetDir)
{
    if (!QDir().mkpath(QFileInfo(zipArchive).absolutePath()))
        return false;

    QSaveFile zipFile(zipArchive);
    if (!zipFile.open(QFile::WriteOnly | QFile::Truncate))
        return false;
    // Creo lo zip
    QuaZip zip(&zipFile);
    if (!zip.open(QuaZip::mdCreate)) {
        return false;
    }

    QDir dir(targetDir);

    // Comprimo i file
    QFileInfo info;
    for (const auto &file : files) {
        info.setFile(file);
        if (!info.isFile() ||
            !compressFile(&zip, info.canonicalFilePath(),
                dir.filePath(info.fileName()))) {
            return false;
        }
    }

    // Chiudo il file zip
    zip.close();
    if (zip.zipError() == ZIP_OK) {
        zipFile.commit();
        return true;
    }

    return false;
}

bool JlCompress::compressDir(
    const QString &zipArchive, const QString &dir, bool recursive)
{
    return compressDir(zipArchive, dir, recursive, 0);
}

bool JlCompress::compressDir(const QString &zipArchive, const QString &dir,
    bool recursive, QDir::Filters filters)
{
    if (!QDir().mkpath(QFileInfo(zipArchive).absolutePath()))
        return false;

    QSaveFile zipFile(zipArchive);
    if (!zipFile.open(QFile::WriteOnly | QFile::Truncate))
        return false;
    // Creo lo zip
    QuaZip zip(&zipFile);
    if (!zip.open(QuaZip::mdCreate)) {
        return false;
    }

    // Aggiungo i file e le sotto cartelle
    if (!compressSubDir(&zip, dir, dir, recursive, filters)) {
        return false;
    }

    // Chiudo il file zip
    zip.close();
    if (zip.zipError() == ZIP_OK) {
        zipFile.commit();
        return true;
    }

    return false;
}

QString JlCompress::extractFile(
    const QString &zipArchive, const QString &fileName, const QString &fileDest)
{
    // Apro lo zip
    QuaZip zip(zipArchive);
    return extractFile(&zip, fileName, fileDest);
}

QString JlCompress::extractFile(
    QuaZip *zip, const QString &fileName, const QString &fileDest)
{
    if (!zip->open(QuaZip::mdUnzip)) {
        return QString();
    }

    if (!extractSingleFile(
            zip, fileName, fileDest.isEmpty() ? fileName : fileDest)) {
        return QString();
    }

    // Chiudo il file zip
    zip->close();
    if (zip->zipError() != UNZ_OK) {
        return QString();
    }

    return fileDest;
}

QStringList JlCompress::extractFiles(const QString &zipArchive,
    const QStringList &files, const QString &targetDir)
{
    // Creo lo zip
    QuaZip zip(zipArchive);
    return extractFiles(&zip, files, targetDir);
}

QStringList JlCompress::extractFiles(
    QuaZip *zip, const QStringList &files, const QString &dir)
{
    if (!zip->open(QuaZip::mdUnzip)) {
        return QStringList();
    }

    // Estraggo i file
    QStringList extracted;
    for (auto &file : files) {
        QString absPath = QDir(dir).absoluteFilePath(file);
        if (!extractSingleFile(zip, file, absPath)) {
            continue;
        }
        extracted.append(absPath);
    }

    // Chiudo il file zip
    zip->close();
    return extracted;
}

QStringList JlCompress::extractDir(
    const QString &zipArchive, const QString &dir, const QString &targetDir)
{
    // Apro lo zip
    QuaZip zip(zipArchive);
    return extractDir(&zip, dir, targetDir);
}

QStringList JlCompress::extractDir(
    QuaZip *zip, const QString &dir, const QString &targetDir)
{
    if (!zip->open(QuaZip::mdUnzip)) {
        return QStringList();
    }
    QDir directory(targetDir);
    QString absCleanDir = QDir::cleanPath(directory.absolutePath());
    QStringList extracted;
    if (!zip->goToFirstFile()) {
        return QStringList();
    }

    QuaZipDir zipDir(zip, dir);
    if (zipDir.isRoot())
        do {
            QString name = zip->currentFilePath();
            QString absFilePath = directory.absoluteFilePath(name);
            QString absCleanPath = QDir::cleanPath(absFilePath);
            if (!absCleanPath.startsWith(absCleanDir + '/'))
                continue;

            if (!extractSingleFile(zip, QString(), absFilePath)) {
                continue;
            }
            extracted.append(absFilePath);
        } while (zip->goToNextFile());
    else {
        zipDir.entryInfoList(QDir::AllDirs);
    }

    // Chiudo il file zip
    zip->close();
    return extracted;
}

QStringList JlCompress::getFileList(const QString &fileCompressed)
{
    // Apro lo zip
    QuaZip zip(fileCompressed);
    return getFileList(&zip);
}

QStringList JlCompress::getFileList(QuaZip *zip)
{
    if (!zip->open(QuaZip::mdUnzip)) {
        return QStringList();
    }

    // Estraggo i nomi dei file
    QStringList lst;
    QuaZipFileInfo info;
    for (bool more = zip->goToFirstFile(); more; more = zip->goToNextFile()) {
        auto filePath = zip->currentFilePath();
        if (filePath.isEmpty())
            break;

        lst << filePath;
    }

    // Chiudo il file zip
    zip->close();
    if (zip->zipError() != UNZ_OK) {
        return QStringList();
    }
    return lst;
}

QStringList JlCompress::extractDir(QIODevice *ioDevice, const QString &dir)
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
    QIODevice *ioDevice, const QString &fileName, const QString &fileDest)
{
    QuaZip zip(ioDevice);
    return extractFile(&zip, fileName, fileDest);
}

QStringList JlCompress::extractFiles(
    QIODevice *ioDevice, const QStringList &files, const QString &dir)
{
    QuaZip zip(ioDevice);
    return extractFiles(&zip, files, dir);
}
