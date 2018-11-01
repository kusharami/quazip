#include "quazipkeysgenerator.h"

#include "minizip_crypt.h"

#include <QTextCodec>

#include <deque>

struct QuaZipKeysGenerator::Private : public QSharedData {
    struct KeyStore {
        QuaZipKeysGenerator::Keys keys;

        KeyStore();
        KeyStore(const QuaZipKeysGenerator::Keys &keys);
        KeyStore(const KeyStore &other);

        KeyStore &operator=(const KeyStore &other);
        inline bool operator==(const KeyStore &other) const;
        inline bool operator!=(const KeyStore &other) const;
    };

    std::deque<KeyStore> keyStack;
    QTextCodec *passwordCodec;
    static QTextCodec *defaultPasswordCodec;
    static const z_crc_t FAR *crcTable;

    Private(QTextCodec *passwordCodec = nullptr);
    Private(const KeyStore &keys, QTextCodec *passwordCodec);
    Private(const Private &other);

    bool equals(const Private &other) const;
    static QTextCodec *getDefaultPasswordCodec();
};

QTextCodec *QuaZipKeysGenerator::Private::defaultPasswordCodec = nullptr;
const z_crc_t FAR *QuaZipKeysGenerator::Private::crcTable = NULL;

QuaZipKeysGenerator::QuaZipKeysGenerator(QTextCodec *passwordCodec)
    : d(new Private(passwordCodec))
{
}

QuaZipKeysGenerator::QuaZipKeysGenerator(
    const QuaZipKeysGenerator::Keys &keys, QTextCodec *passwordCodec)
    : d(new Private(keys, passwordCodec))
{
}

QuaZipKeysGenerator::QuaZipKeysGenerator(const QuaZipKeysGenerator &other)
{
    operator=(other);
}

QuaZipKeysGenerator::~QuaZipKeysGenerator()
{
    // destruct private
}

QTextCodec *QuaZipKeysGenerator::defaultPasswordCodec()
{
    return Private::getDefaultPasswordCodec();
}

void QuaZipKeysGenerator::setDefaultPasswordCodec(QTextCodec *codec)
{
    Private::defaultPasswordCodec = codec;
}

QTextCodec *QuaZipKeysGenerator::passwordCodec() const
{
    return d->passwordCodec;
}

void QuaZipKeysGenerator::setPasswordCodec(QTextCodec *codec)
{
    d->passwordCodec = codec ? codec : Private::getDefaultPasswordCodec();
}

const QuaZipKeysGenerator::Keys &QuaZipKeysGenerator::keys() const
{
    return d->keyStack.back().keys;
}

void QuaZipKeysGenerator::setKeys(const QuaZipKeysGenerator::Keys &keys)
{
    d->keyStack.clear();
    d->keyStack.emplace_back(keys);
}

void QuaZipKeysGenerator::resetKeys()
{
    d->keyStack.clear();
    d->keyStack.emplace_back();
}

int QuaZipKeysGenerator::addPassword(QString &unicode)
{
    QByteArray mbcs = d->passwordCodec->fromUnicode(unicode);
    memset(unicode.data(), 0, size_t(unicode.length()) * sizeof(QChar));
    unicode.clear();
    return addPassword(mbcs);
}

int QuaZipKeysGenerator::addPassword(QByteArray &mbcs)
{
    for (char ch : mbcs) {
        addPassword(ch);
    }

    int result = mbcs.length();
    memset(mbcs.data(), 0, size_t(result));
    mbcs.clear();

    return result;
}

int QuaZipKeysGenerator::addPassword(QChar unicode)
{
    QByteArray mbcs = d->passwordCodec->fromUnicode(unicode);
    return addPassword(mbcs);
}

void QuaZipKeysGenerator::addPassword(char ch)
{
    auto &keyStack = d->keyStack;
    keyStack.emplace_back(keyStack.back());
    auto keys = keyStack.back().keys;
    update_keys(keys, Private::crcTable, ch);
}

void QuaZipKeysGenerator::rollback(int countBytes)
{
    if (countBytes < 0) {
        resetKeys();
        return;
    }
    auto &keyStack = d->keyStack;
    while (keyStack.size() > 1 && countBytes-- > 0) {
        keyStack.pop_back();
    }
}

QuaZipKeysGenerator &QuaZipKeysGenerator::operator=(
    const QuaZipKeysGenerator &other)
{
    d = other.d;
    return *this;
}

bool QuaZipKeysGenerator::operator==(const QuaZipKeysGenerator &other) const
{
    return d == other.d || d->equals(*other.d.data());
}

QuaZipKeysGenerator::Private::Private(QTextCodec *passwordCodec)
    : Private(KeyStore(), passwordCodec)
{
}

QuaZipKeysGenerator::Private::Private(
    const KeyStore &keys, QTextCodec *passwordCodec)
{
    if (crcTable == NULL)
        crcTable = get_crc_table();

    if (!passwordCodec)
        passwordCodec = getDefaultPasswordCodec();
    this->passwordCodec = passwordCodec;

    keyStack.emplace_back(keys);
}

QuaZipKeysGenerator::Private::Private(const Private &other)
    : keyStack(other.keyStack)
    , passwordCodec(other.passwordCodec)
{
}

bool QuaZipKeysGenerator::Private::equals(const Private &other) const
{
    return passwordCodec == other.passwordCodec && keyStack == other.keyStack;
}

QTextCodec *QuaZipKeysGenerator::Private::getDefaultPasswordCodec()
{
    if (!defaultPasswordCodec) {
        defaultPasswordCodec = QTextCodec::codecForLocale();
    }
    return defaultPasswordCodec;
}

QuaZipKeysGenerator::Private::KeyStore::KeyStore()
{
    reset_keys(keys);
}

QuaZipKeysGenerator::Private::KeyStore::KeyStore(const Keys &k)
{
    memcpy(keys, k, sizeof(Keys));
}

QuaZipKeysGenerator::Private::KeyStore::KeyStore(const KeyStore &other)
{
    operator=(other);
}

QuaZipKeysGenerator::Private::KeyStore &QuaZipKeysGenerator::Private::KeyStore::
operator=(const KeyStore &other)
{
    memcpy(keys, other.keys, sizeof(Keys));
    return *this;
}

bool QuaZipKeysGenerator::Private::KeyStore::operator==(
    const KeyStore &other) const
{
    return 0 == memcmp(keys, other.keys, sizeof(Keys));
}

bool QuaZipKeysGenerator::Private::KeyStore::operator!=(
    const KeyStore &other) const
{
    return !operator==(other);
}
