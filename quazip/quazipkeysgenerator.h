#pragma once

#include "quazip_global.h"

#include <QSharedData>

class QTextCodec;
class QByteArray;
class QString;

class QUAZIP_EXPORT QuaZipKeysGenerator {
public:
    enum
    {
        KEY_COUNT = 3
    };
    using Keys = unsigned long[KEY_COUNT];

    QuaZipKeysGenerator(QTextCodec *passwordCodec = nullptr);
    QuaZipKeysGenerator(const Keys &keys, QTextCodec *passwordCodec = nullptr);
    QuaZipKeysGenerator(const QuaZipKeysGenerator &other);
    ~QuaZipKeysGenerator();

    static QTextCodec *defaultPasswordCodec();
    static void setDefaultPasswordCodec(QTextCodec *codec = nullptr);

    QTextCodec *passwordCodec() const;
    void setPasswordCodec(QTextCodec *codec);

    const Keys &keys() const;
    void setKeys(const Keys &keys);

    void resetKeys();
    int addPassword(QString &unicode);
    int addPassword(QByteArray &mbcs);
    int addPassword(QChar unicode);
    void addPassword(char ch);
    void rollback(int countBytes);

    QuaZipKeysGenerator &operator=(const QuaZipKeysGenerator &other);

    bool operator==(const QuaZipKeysGenerator &other) const;
    inline bool operator!=(const QuaZipKeysGenerator &other) const;

private:
    struct Private;
    QSharedDataPointer<Private> d;
};

bool QuaZipKeysGenerator::operator!=(const QuaZipKeysGenerator &other) const
{
    return !operator==(other);
}
