#pragma once

#include "quazip_global.h"

#include <QTextCodec>

class QuaZipTextCodecPrivate;

/// Class for encoding/decoding file names and comments in a zip file.
/**
 * \note On Windows you can convert with any valid code page number;
 * on other systems only these code pages are supported:
 * 65001, 1200, 1201, 12000, 12001, 1250-1258, 850, 866, 874, 932,
 * 949, 950, 20932, 51932, 51949, 54936, 10000, 28591-28599, 28603, 28606
 * 20866, 21866, 50220, 50221, 50222
 */
class QUAZIP_EXPORT QuaZipTextCodec : public QTextCodec {
public:
    /// Default constructor
    /**
     * On Windows it uses current OEM code page if it is not UTF-7 or UTF-8.
     * In other cases and on other systems it uses
     * IBM 437 code page (if available) or IBM 850 code page.
     **/
    QuaZipTextCodec();
    /// Constructs codec using Windows code page number
    /**
     * @param codepage Valid Windows code page number
     * If zero, then default behavior.
     *
     * \sa setCodePage()
     */
    QuaZipTextCodec(quint32 codepage);

    virtual ~QuaZipTextCodec() override;

    /// Windows code page number
    /// \return Zero if default behavior
    quint32 codepage() const;
    /// Set Windows code page number
    /**
     * @param codepage Valid Windows code page number.
     * If zero, then default behavior.
     **/
    void setCodePage(quint32 codepage);

    /// Returns QTextCodec for supported Windows Code Page number
    static QTextCodec *codecForCodePage(quint32 codepage);

    /// Converts Windows Code Page Number to IANA Code Page number
    static int codePageToMib(quint32 codepage);

    /// Converts IANA Code Page Number to Windows Code Page number
    static quint32 mibToCodePage(int mib);

    virtual QByteArray name() const override;
    virtual QList<QByteArray> aliases() const override;
    virtual int mibEnum() const override;

    /// Supported Windows code page numbers
    enum
    {
        WCP_UTF8 = 65001,
        WCP_UTF16LE = 1200,
        WCP_UTF16BE = 1201,
        WCP_UTF32LE = 12000,
        WCP_UTF32BE = 12001,
        WCP_IBM437 = 437,
        WCP_IBM850 = 850,
        WCP_IBM866 = 866,
        WCP_IBM874 = 874,
        WCP_OEM_KOR = 949,
        WCP_1250 = 1250,
        WCP_1251 = 1251,
        WCP_1252 = 1252,
        WCP_1253 = 1253,
        WCP_1254 = 1254,
        WCP_1255 = 1255,
        WCP_1256 = 1256,
        WCP_1257 = 1257,
        WCP_1258 = 1258,
        WCP_MACINTOSH = 10000,
        WCP_KOI8R = 20866,
        WCP_KOI8U = 21866,
        WCP_ISO8859_1 = 28591,
        WCP_ISO8859_2 = 28592,
        WCP_ISO8859_3 = 28593,
        WCP_ISO8859_4 = 28594,
        WCP_ISO8859_5 = 28595,
        WCP_ISO8859_6 = 28596,
        WCP_ISO8859_7 = 28597,
        WCP_ISO8859_8 = 28598,
        WCP_ISO8859_9 = 28599,
        WCP_ISO8859_13 = 28603,
        WCP_ISO8859_16 = 28606,
        WCP_SHIFT_JIS = 932,
        WCP_BIG5_HKSCS = 950,
        WCP_EUC_JP_OLD = 20932,
        WCP_EUC_JP = 51932,
        WCP_ISO2022JP0 = 50220,
        WCP_ISO2022JP1 = 50221,
        WCP_ISO2022JP = 50222,
        WCP_EUC_KR = 51949,
        WCP_GB18030 = 54936,
    };

    /// Supported IANA code page numbers
    enum
    {
        IANA_UTF8 = 106,
        IANA_UTF16LE = 1014,
        IANA_UTF16BE = 1013,
        IANA_UTF32LE = 1019,
        IANA_UTF32BE = 1018,
        IANA_IBM437 = 2011,
        IANA_IBM850 = 2009,
        IANA_IBM866 = 2086,
        IANA_IBM874 =
            -874, // Fake number. Perhaps it should be 2108 windows-874?
        IANA_OEM_KOR = -949, // Fake number. Not in IANA
        IANA_1250 = 2250,
        IANA_1251 = 2251,
        IANA_1252 = 2252,
        IANA_1253 = 2253,
        IANA_1254 = 2254,
        IANA_1255 = 2255,
        IANA_1256 = 2256,
        IANA_1257 = 2257,
        IANA_1258 = 2258,
        IANA_MACINTOSH = 2027,
        IANA_KOI8R = 2084,
        IANA_KOI8U = 2088,
        IANA_ISO8859_1 = 4,
        IANA_ISO8859_2 = 5,
        IANA_ISO8859_3 = 6,
        IANA_ISO8859_4 = 7,
        IANA_ISO8859_5 = 8,
        IANA_ISO8859_6 = 9,
        IANA_ISO8859_7 = 10,
        IANA_ISO8859_8 = 11,
        IANA_ISO8859_9 = 12,
        IANA_ISO8859_13 = 109,
        IANA_ISO8859_16 = 112,
        IANA_SHIFT_JIS = 17,
        IANA_BIG5_HKSCS = 2101,
        IANA_EUC_JP = 18,
        IANA_ISO2022JP = 39,
        IANA_EUC_KR = 38,
        IANA_GB18030 = 114,
    };

protected:
    virtual QString convertToUnicode(
        const char *in, int length, ConverterState *state) const override;
    virtual QByteArray convertFromUnicode(
        const QChar *in, int length, ConverterState *state) const override;

private:
    QuaZipTextCodecPrivate *d;
};
