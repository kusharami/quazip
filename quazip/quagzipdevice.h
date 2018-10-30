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

#include "quaziodevice.h"
#include "quazextrafield.h"

#include <QMap>
#include <ctime>

class QuaGzipDevicePrivate;
class QTextCodec;

/// A class to compress/decompress Gzip file data.
/**
  This class can be used to compress any data in Gzip file format written to
  QIODevice or decompress it back.
  Compressing data sent over a QTcpSocket is a good example.
  */
class QUAZIP_EXPORT QuaGzipDevice : public QuaZIODevice {
    Q_OBJECT

public:
    /// Constructor.
    /**
      \param parent The parent object, as per QObject logic.
      */
    explicit QuaGzipDevice(QObject *parent = nullptr);
    /// Constructor.
    /**
      \param io The QIODevice to read/write.
      \param parent The parent object, as per QObject logic.
      */
    explicit QuaGzipDevice(QIODevice *io, QObject *parent = nullptr);
    virtual ~QuaGzipDevice() override;

    /// Returns maximum length for stored file name
    static int maxFileNameLength();
    /// Returns maximum length for stored comment
    static int maxCommentLength();

    /// Returns true if Gz header acquired to access
    /// stored file name, comment, modification time and extra fields.
    /// Always true in write mode.
    bool headerIsProcessed() const;

    /// What codec is used to convert stored original file name.
    /// Default is based on locale.
    QTextCodec *fileNameCodec() const;
    /// Set what codec is used to convert original file name.
    /**
      \param codec Unicode text conversion codec. Cannot be null.
    */
    void setFileNameCodec(QTextCodec *codec);
    /// Set what codec is used to convert original file name.
    /**
      \param codecName Valid unicode text conversion codec name
    */
    void setFileNameCodec(const char *codecName);

    /// What codec is used to convert stored comment.
    /// Default is based on locale.
    QTextCodec *commentCodec() const;
    /// Set what codec is used to convert comment.
    /**
      \param codec Unicode text conversion codec. Cannot be null.
    */
    void setCommentCodec(QTextCodec *codec);
    /// Set what codec is used to convert comment.
    /**
      \param codecName Valid unicode text conversion codec name
    */
    void setCommentCodec(const char *codecName);

    /// Sets original file name provided by getIODevice() if it is QFileDevice.
    /// Does nothing if original file name is already set.
    void restoreOriginalFileName();

    /// Stored original file name in a compressed device.
    /// When reading, check headerIsProcessed() first.
    QString originalFileName() const;
    /// Original file name to store in a compressed device
    /**
      \param fileName The original file name.
        Converted to multibyte string with filePathCodec().
        Generates error if cannot be encoded or longer than maxFileNameLength().
     */
    void setOriginalFileName(const QString &fileName);

    /// Stored comment in a compressed device.
    /// When reading, check headerIsProcessed() first.
    QString comment() const;
    /// Comment to store in a compressed device
    /**
      \param text Custom commentary text.
        Converted to multibyte string with commentCodec().
        Generates error if cannot be encoded or longer than maxCommentLength().
     */
    void setComment(const QString &text);

    /// Stored modification time in a compressed device.
    /// When reading, check headerIsProcessed() first.
    time_t modificationTime() const;
    /// Modification time to store in a compressed device
    /**
      \param time Time stamp in UTC.
        If zero, current time is set when opened for write.
     */
    void setModificationTime(time_t time);

    /// Stored extra fields in a compressed device.
    /// When reading, check headerIsProcessed() first.
    QuaZExtraField::Map extraFields() const;
    /// Extra fields to store in a compressed device
    /**
      \param map Map of fields and values.
        Generates error if extra fields is too big.
     */
    void setExtraFields(const QuaZExtraField::Map &map);

private:
    inline QuaGzipDevicePrivate *d() const;
};
