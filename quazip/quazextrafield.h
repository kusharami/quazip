#pragma once

#include <QMap>
#include <QByteArray>

#include "quazip_global.h"

class QIODevice;

/// Utility class to provide Extra Field access by key
struct QUAZIP_EXPORT QuaZExtraField {
    struct Key {
        char key[2];

        inline Key();
        inline Key(const char *si);
        inline Key(quint16 id);
        inline Key(char si1, char si2);
        inline Key(const Key &other);

        inline Key &operator=(const Key &other);

        inline bool operator==(const Key &other) const;
        inline bool operator!=(const Key &other) const;
        inline bool operator<(const Key &other) const;
        inline bool operator>(const Key &other) const;
        inline bool operator<=(const Key &other) const;
        inline bool operator>=(const Key &other) const;

        /// Numeric id of the key
        inline quint16 id() const;
        /// Set numeric id of the key
        inline void setId(quint16 id);

        inline operator quint16() const;
    };

    /// Result codes for toMap() and fromMap()
    enum ResultCode
    {
        /// On success
        OK,
        /// When field data size is too big
        ERR_FIELD_SIZE_LIMIT,
        /// When joined fields data is too big
        ERR_BUFFER_SIZE_LIMIT,
        /// When Extra Field data is corrupted
        ERR_CORRUPTED_DATA,
        /// When unable to read from a source device
        ERR_DEVICE_READ_FAILED,
        /// When unable to write to a target device
        ERR_DEVICE_WRITE_FAILED
    };

    /// Maps Extra Field data by 2-characters key
    using Map = QMap<QuaZExtraField::Key, QByteArray>;

    /// Converts Extra Field data to QuaZExtraField::Map
    /**
     * @param data Extra Field data
     */
    static Map toMap(const QByteArray &data);
    /// Converts Extra Field data to QuaZExtraField::Map
    /**
     * @param data Extra Field data pointer, cannot be NULL
     * @param length Length of the Extra Field data
     */
    static Map toMap(const void *data, int length);

    /// Converts Extra Field data to QuaZExtraField::Map
    /**
     * @param targetMap Target map to store extra field data
     * @param sourceDevice Device to read Extra Field data
     * @param length Length of the source data,
     * if -1, sourceDevice->size() is used
     * @return ResultCode value
     */
    static ResultCode toMap(
        Map &targetMap, QIODevice *sourceDevice, int length = -1);

    /// Converts Extra Field map to a single buffer
    /**
     * @param map Map of extra field data by its key
     * @param resultCode if not NULL, stores OK if success or ERR \sa ResultCode
     * @param maxSize Maxium size of resulting data
     * @return Resulting Extra field data that can be to archive
     */
    static QByteArray fromMap(
        const Map &map, ResultCode *resultCode = nullptr, int maxSize = -1);

    /// Writes Extra Field data from a map
    /**
     * @param targetDevice Target device to store Extra Field data
     * @param map Map of Extra Field data
     * @param maxSize Maximum size to be stored
     * @return ResultCode value
     */
    static ResultCode fromMap(
        QIODevice *targetDevice, const Map &map, int maxSize = -1);

    /// Returns the error string associated with \a code
    static QString errorString(ResultCode code);
};

QuaZExtraField::Key::Key()
{
    key[0] = 0;
    key[1] = 0;
}

QuaZExtraField::Key::Key(const char *si)
{
    key[0] = si ? si[0] : char(0);
    key[1] = (si[0] == 0) ? char(0) : si[1];
}

QuaZExtraField::Key::Key(quint16 value)
{
    setId(value);
}

QuaZExtraField::Key::Key(char si1, char si2)
{
    key[0] = si1;
    key[1] = si2;
}

QuaZExtraField::Key::Key(const Key &other)
{
    operator=(other);
}

QuaZExtraField::Key &QuaZExtraField::Key::operator=(const Key &other)
{
    key[0] = other.key[0];
    key[1] = other.key[1];
    return *this;
}

bool QuaZExtraField::Key::operator==(const Key &other) const
{
    return key[0] == other.key[0] && key[1] == other.key[1];
}

bool QuaZExtraField::Key::operator!=(const Key &other) const
{
    return !operator==(other);
}

bool QuaZExtraField::Key::operator<(const Key &other) const
{
    return id() < other.id();
}

bool QuaZExtraField::Key::operator>(const Key &other) const
{
    return id() > other.id();
}

bool QuaZExtraField::Key::operator<=(const Key &other) const
{
    return id() <= other.id();
}

bool QuaZExtraField::Key::operator>=(const Key &other) const
{
    return id() >= other.id();
}

quint16 QuaZExtraField::Key::id() const
{
    return quint16((key[1] << 8) | key[0]);
}

void QuaZExtraField::Key::setId(quint16 id)
{
    key[0] = char(id);
    key[1] = char(id >> 8);
}

QuaZExtraField::Key::operator quint16() const
{
    return id();
}
