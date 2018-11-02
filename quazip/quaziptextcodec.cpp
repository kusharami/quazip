#include "quaziptextcodec.h"

#ifdef Q_OS_WIN
#include "Windows.h"
#undef max
#include <vector>
#endif

#include <QLocale>

class QuaZipTextCodec::Private {
public:
    QTextCodec *customCodec;
    quint32 codepage;

    Private(quint32 codepage);

    QByteArray codecName() const;
    void setCodePage(quint32 codepage);
    bool shouldUseCustomCodec();

    static QByteArray codecNameForCodePage(quint32 codepage);
};

#ifdef Q_OS_WIN
inline DWORD getMBtoWCFlagsForCodePage(int codepage)
{
    switch (codepage) {
    case 50220:
    case 50221:
    case 50222:
    case 50225:
    case 50227:
    case 50229:
    case 57002:
    case 57003:
    case 57004:
    case 57005:
    case 57006:
    case 57007:
    case 57008:
    case 57009:
    case 57010:
    case 57011:
    case CP_UTF7:
    case CP_SYMBOL:
        return 0;

    default:
        break;
    }
    return MB_PRECOMPOSED;
}

inline DWORD getWCtoMBFlagsForCodePage(int codepage)
{
    switch (codepage) {
    case 50220:
    case 50221:
    case 50222:
    case 50225:
    case 50227:
    case 50229:
    case 57002:
    case 57003:
    case 57004:
    case 57005:
    case 57006:
    case 57007:
    case 57008:
    case 57009:
    case 57010:
    case 57011:
    case CP_UTF7:
    case CP_SYMBOL:
        return 0;

    default:
        break;
    }
    return WC_NO_BEST_FIT_CHARS | WC_COMPOSITECHECK | WC_DEFAULTCHAR;
}

inline DWORD getMBtoWCErrorFlagsForCodePage(int codepage)
{
    switch (codepage) {
    case CP_UTF8:
    case 54936:
        return MB_ERR_INVALID_CHARS;
    }

    return 0;
}

inline DWORD getWCtoMBErrorFlagsForCodePage(int codepage)
{
    switch (codepage) {
    case CP_UTF8:
    case 54936:
        return WC_ERR_INVALID_CHARS;
    }

    return 0;
}
#endif

QuaZipTextCodec::QuaZipTextCodec(quint32 codepage)
    : d(new Private(codepage))
{
}

QuaZipTextCodec::~QuaZipTextCodec()
{
    delete d;
}

quint32 QuaZipTextCodec::codepage() const
{
#ifdef Q_OS_WIN
    if (d->codepage == 0)
        return GetOEMCP();
#endif

    return d->codepage;
}

QTextCodec *QuaZipTextCodec::codecForCodepage(quint32 codepage)
{
    int mib = mibForCodepage(codepage);
    if (mib == 0) {
        auto codec = codecForName(Private::codecNameForCodePage(codepage));
        if (!codec)
            codec = new QuaZipTextCodec(codepage);
        return codec;
    }

    return codecForMib(mib);
}

QTextCodec *QuaZipTextCodec::codecForLocale()
{
    auto codec = QTextCodec::codecForLocale();
    Q_ASSERT(codec);
#ifdef Q_OS_WIN
    if (codec->mibEnum() == 0) {
        return codecForCodepage(GetACP());
    }
#endif
    return codec;
}

quint32 QuaZipTextCodec::codepageForCodec(QTextCodec *codec)
{
    if (!codec)
        codec = 0;

    auto zCodec = dynamic_cast<QuaZipTextCodec *>(codec);
    if (zCodec) {
        return zCodec->codepage();
    }

    int codepage = codepageForMib(codec->mibEnum());
    if (codepage == 0) {
#ifdef Q_OS_WIN
        return GetACP();
#else
        return WCP_UTF8;
#endif
    }
    return codepage;
}

int QuaZipTextCodec::mibForCodepage(quint32 codepage)
{
    switch (codepage) {
    case WCP_UTF8:
        return IANA_UTF8;
    case WCP_UTF16LE:
        return IANA_UTF16LE;
    case WCP_UTF16BE:
        return IANA_UTF16BE;
    case WCP_UTF32LE:
        return IANA_UTF32LE;
    case WCP_UTF32BE:
        return IANA_UTF32BE;
    case WCP_IBM437:
        return IANA_IBM437;
    case WCP_IBM850:
        return IANA_IBM850;
    case WCP_IBM866:
        return IANA_IBM866;
    case WCP_IBM874:
        return IANA_IBM874;
    case WCP_OEM_KOR:
        return IANA_OEM_KOR;
    case WCP_1250:
        return IANA_1250;
    case WCP_1251:
        return IANA_1251;
    case WCP_1252:
        return IANA_1252;
    case WCP_1253:
        return IANA_1253;
    case WCP_1254:
        return IANA_1254;
    case WCP_1255:
        return IANA_1255;
    case WCP_1256:
        return IANA_1256;
    case WCP_1257:
        return IANA_1257;
    case WCP_1258:
        return IANA_1258;
    case WCP_MACINTOSH:
        return IANA_MACINTOSH;
    case WCP_KOI8R:
        return IANA_KOI8R;
    case WCP_KOI8U:
        return IANA_KOI8U;
    case WCP_ISO8859_1:
        return IANA_ISO8859_1;
    case WCP_ISO8859_2:
        return IANA_ISO8859_2;
    case WCP_ISO8859_3:
        return IANA_ISO8859_3;
    case WCP_ISO8859_4:
        return IANA_ISO8859_4;
    case WCP_ISO8859_5:
        return IANA_ISO8859_5;
    case WCP_ISO8859_6:
        return IANA_ISO8859_6;
    case WCP_ISO8859_7:
        return IANA_ISO8859_7;
    case WCP_ISO8859_8:
        return IANA_ISO8859_8;
    case WCP_ISO8859_9:
        return IANA_ISO8859_9;
    case WCP_ISO8859_13:
        return IANA_ISO8859_13;
    case WCP_ISO8859_16:
        return IANA_ISO8859_16;
    case WCP_SHIFT_JIS:
        return IANA_SHIFT_JIS;
    case WCP_BIG5_HKSCS:
        return IANA_BIG5_HKSCS;
    case WCP_EUC_JP_OLD:
    case WCP_EUC_JP:
        return IANA_EUC_JP;
    case WCP_ISO2022JP0:
    case WCP_ISO2022JP1:
    case WCP_ISO2022JP:
        return IANA_ISO2022JP;
    case WCP_EUC_KR:
        return IANA_EUC_KR;
    case WCP_GB18030:
        return IANA_GB18030;
    }

    return 0;
}

quint32 QuaZipTextCodec::codepageForMib(int mib)
{
    switch (mib) {
    case IANA_UTF8:
        return WCP_UTF8;
    case IANA_UTF16LE:
        return WCP_UTF16LE;
    case IANA_UTF16BE:
        return WCP_UTF16BE;
    case IANA_UTF32LE:
        return WCP_UTF32LE;
    case IANA_UTF32BE:
        return WCP_UTF32BE;
    case IANA_IBM437:
        return WCP_IBM437;
    case IANA_IBM850:
        return WCP_IBM850;
    case IANA_IBM866:
        return WCP_IBM866;
    case IANA_IBM874:
        return WCP_IBM874;
    case IANA_OEM_KOR:
        return WCP_OEM_KOR;
    case IANA_1250:
        return WCP_1250;
    case IANA_1251:
        return WCP_1251;
    case IANA_1252:
        return WCP_1252;
    case IANA_1253:
        return WCP_1253;
    case IANA_1254:
        return WCP_1254;
    case IANA_1255:
        return WCP_1255;
    case IANA_1256:
        return WCP_1256;
    case IANA_1257:
        return WCP_1257;
    case IANA_1258:
        return WCP_1258;
    case IANA_MACINTOSH:
        return WCP_MACINTOSH;
    case IANA_KOI8R:
        return WCP_KOI8R;
    case IANA_KOI8U:
        return WCP_KOI8U;
    case IANA_ISO8859_1:
        return WCP_ISO8859_1;
    case IANA_ISO8859_2:
        return WCP_ISO8859_2;
    case IANA_ISO8859_3:
        return WCP_ISO8859_3;
    case IANA_ISO8859_4:
        return WCP_ISO8859_4;
    case IANA_ISO8859_5:
        return WCP_ISO8859_5;
    case IANA_ISO8859_6:
        return WCP_ISO8859_6;
    case IANA_ISO8859_7:
        return WCP_ISO8859_7;
    case IANA_ISO8859_8:
        return WCP_ISO8859_8;
    case IANA_ISO8859_9:
        return WCP_ISO8859_9;
    case IANA_ISO8859_13:
        return WCP_ISO8859_13;
    case IANA_ISO8859_16:
        return WCP_ISO8859_16;
    case IANA_SHIFT_JIS:
        return WCP_SHIFT_JIS;
    case IANA_BIG5_HKSCS:
        return WCP_BIG5_HKSCS;
    case IANA_EUC_JP:
        return WCP_EUC_JP;
    case IANA_ISO2022JP:
        return WCP_ISO2022JP;
    case IANA_EUC_KR:
        return WCP_EUC_KR;
    case IANA_GB18030:
        return WCP_GB18030;
    }

    return 0;
}

QByteArray QuaZipTextCodec::name() const
{
    return d->codecName();
}

QList<QByteArray> QuaZipTextCodec::aliases() const
{
    return QList<QByteArray>();
}

int QuaZipTextCodec::mibEnum() const
{
    return std::numeric_limits<int>::max();
}

QString QuaZipTextCodec::convertToUnicode(
    const char *in, int length, ConverterState *state) const
{
    QString result;
    if (!in || length <= 0)
        return result;

    if (d->shouldUseCustomCodec()) {
        return d->customCodec->toUnicode(in, length, state);
    }
#ifdef Q_OS_WIN
    static_assert(sizeof(QChar) == sizeof(WCHAR), "WCHAR size mismatch");

    quint32 cp = codepage();

    DWORD flags = getMBtoWCFlagsForCodePage(cp);
    bool validate = state && state->flags.testFlag(ConvertInvalidToNull);
    if (validate)
        flags |= getMBtoWCErrorFlagsForCodePage(cp);

    int resultLength = MultiByteToWideChar(cp, flags, in, length, NULL, 0);

    if (resultLength <= 0) {
        if (state && GetLastError() == ERROR_NO_UNICODE_TRANSLATION) {
            state->remainingChars -= length;
            state->invalidChars += length;
        }
        return QString();
    }

    std::vector<WCHAR> unicode;
    unicode.resize(size_t(resultLength));
    resultLength = MultiByteToWideChar(
        cp, flags, in, length, unicode.data(), resultLength);
    result = QString::fromWCharArray(unicode.data(), resultLength);
#else
    if (state) {
        state->remainingChars -= length;
        state->invalidChars += length;
    }
#endif
    return result;
}

QByteArray QuaZipTextCodec::convertFromUnicode(
    const QChar *in, int length, ConverterState *state) const
{
    QByteArray result;
    if (!in || length <= 0)
        return result;

    if (d->shouldUseCustomCodec()) {
        return d->customCodec->fromUnicode(in, length, state);
    }

#ifdef Q_OS_WIN
    static_assert(sizeof(QChar) == sizeof(WCHAR), "WCHAR size mismatch");

    bool validate = state && state->flags.testFlag(ConvertInvalidToNull);

    auto cp = codepage();

    DWORD flags = getWCtoMBFlagsForCodePage(cp);
    if (validate)
        flags |= getWCtoMBErrorFlagsForCodePage(cp);

    BOOL usedDefaultCharacter = false;
    LPBOOL usedDefaultCharacterP = nullptr;
    if (cp != CP_UTF7 && cp != CP_UTF8) {
        usedDefaultCharacterP = &usedDefaultCharacter;
    }
    int resultLength =
        WideCharToMultiByte(cp, flags, reinterpret_cast<const WCHAR *>(in),
            length, NULL, 0, NULL, usedDefaultCharacterP);

    if (usedDefaultCharacter) {
        if (validate) {
            state->remainingChars -= length;
            state->invalidChars += length;
            return QByteArray();
        }
    }

    if (resultLength <= 0) {
        if (validate && GetLastError() == ERROR_NO_UNICODE_TRANSLATION) {
            state->remainingChars -= length;
            state->invalidChars += length;
        }
        return QByteArray();
    }

    result.resize(resultLength);
    resultLength =
        WideCharToMultiByte(cp, flags, reinterpret_cast<const WCHAR *>(in),
            length, result.data(), resultLength, NULL, NULL);
#else
    if (state) {
        state->remainingChars -= length;
        state->invalidChars += length;
    }
#endif
    return result;
}

QuaZipTextCodec::Private::Private(quint32 codepage)
    : customCodec(nullptr)
    , codepage(codepage)
{
}

QByteArray QuaZipTextCodec::Private::codecName() const
{
    return codecNameForCodePage(codepage);
}

void QuaZipTextCodec::Private::setCodePage(quint32 codepage)
{
    customCodec = nullptr;
    if (codepage == 0) {
        switch (QLocale().language()) {
        case QLocale::Russian:
        case QLocale::Ukrainian:
        case QLocale::Belarusian:
            customCodec =
                QuaZipTextCodec::codecForCodepage(QuaZipTextCodec::WCP_IBM866);
            break;
        default:
            customCodec =
                QuaZipTextCodec::codecForCodepage(QuaZipTextCodec::WCP_IBM437);
            if (!customCodec) {
                customCodec = QuaZipTextCodec::codecForCodepage(
                    QuaZipTextCodec::WCP_IBM850);
            }
            break;
        }
    } else {
        customCodec = QuaZipTextCodec::codecForCodepage(codepage);
    }
}

bool QuaZipTextCodec::Private::shouldUseCustomCodec()
{
#ifdef Q_OS_WIN
    if (codepage == 0)
        return false;
#endif

    if (customCodec == nullptr)
        setCodePage(codepage);

    return customCodec != nullptr;
}

QByteArray QuaZipTextCodec::Private::codecNameForCodePage(quint32 codepage)
{
    return QByteArrayLiteral("QuaZipTextCodec") + QByteArray::number(codepage);
}
