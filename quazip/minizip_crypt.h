/* crypt.h -- base code for crypt/uncrypt ZIPfile


   Version 1.01e, February 12th, 2005

   Copyright (C) 1998-2005 Gilles Vollant

   This code is a modified version of crypting code in Infozip distribution

   The encryption/decryption parts of this source code (as opposed to the
   non-echoing password parts) were originally written in Europe.  The
   whole source package can be freely distributed, including from the USA.
   (Prior to January 2000, re-export from the US was a violation of US law.)

   This encryption code is a direct transcription of the algorithm from
   Roger Schlafly, described by Phil Katz in the file appnote.txt.  This
   file (appnote.txt) is distributed with the PKZIP program (even in the
   version without encryption capabilities).

   If you don't need crypting in your application, just define symbols
   NOCRYPT and NOUNCRYPT.

   This code support the "Traditional PKWARE Encryption".

   The new AES encryption added on Zip format by Winzip (see the page
   http://www.winzip.com/aes_info.htm ) and PKWare PKZip 5.x Strong
   Encryption is not supported.
*/

#include "quazip_global.h"
#include "zlib.h"

#define CRC32(c, b) ((*(pcrc_32_tab + (((int) (c) ^ (b)) & 0xff))) ^ ((c) >> 8))
#define CRYPT_KEY_COUNT 3

/***********************************************************************
 * Return the next byte in the pseudo-random sequence
 */
inline int decrypt_byte(unsigned long *pkeys)
{
    unsigned temp;
    temp = ((unsigned) (*(pkeys + 2)) & 0xffff) | 2;
    return (int) (((temp * (temp ^ 1)) >> 8) & 0xff);
}

/***********************************************************************
 * Update the encryption keys with the next byte of plain text
 */
inline int update_keys(
    unsigned long *pkeys, const z_crc_t FAR *pcrc_32_tab, int c)
{
    (*(pkeys + 0)) = CRC32((*(pkeys + 0)), c);
    (*(pkeys + 1)) += (*(pkeys + 0)) & 0xff;
    (*(pkeys + 1)) = (*(pkeys + 1)) * 134775813L + 1;
    {
        register int keyshift = (int) ((*(pkeys + 1)) >> 24);
        (*(pkeys + 2)) = CRC32((*(pkeys + 2)), keyshift);
    }
    return c;
}

/***********************************************************************
 * Initialize the encryption keys and the random header according to
 * the given password.
 */
inline void reset_keys(unsigned long *pkeys)
{
    *(pkeys + 0) = 305419896L;
    *(pkeys + 1) = 591751049L;
    *(pkeys + 2) = 878082192L;
}

inline void update_keys_pwd(const char *passwd, unsigned long *pkeys,
                            const z_crc_t FAR *pcrc_32_tab)
{
    while (*passwd != '\0') {
        update_keys(pkeys, pcrc_32_tab, (int) *passwd);
        passwd++;
    }
}

inline void init_keys(
    const char *passwd, unsigned long *pkeys, const z_crc_t FAR *pcrc_32_tab)
{
    reset_keys(pkeys);
    update_keys_pwd(passwd, pkeys, pcrc_32_tab);
}

#define zdecode(pkeys, pcrc_32_tab, c) \
    (update_keys(pkeys, pcrc_32_tab, c ^= decrypt_byte(pkeys)))

#define zencode(pkeys, pcrc_32_tab, c, t) \
    (t = decrypt_byte(pkeys), update_keys(pkeys, pcrc_32_tab, c), t ^ (c))

#define RAND_HEAD_LEN 12
    /* "last resort" source for second part of crypt seed pattern */
#ifndef ZCR_SEED2
#define ZCR_SEED2 3141592654UL /* use PI as default pattern */
#endif

inline void crypthead_keys(unsigned char *buf,
    unsigned long *pkeys, const z_crc_t FAR *pcrc_32_tab,
    unsigned long crcForCrypting)
{
    int n; /* index in random header */
    int t; /* temporary */
    int c; /* random byte */
    unsigned char header[RAND_HEAD_LEN - 2]; /* random header */
    static unsigned calls = 0; /* ensure different random header each time */

    /* First generate RAND_HEAD_LEN-2 random bytes. We encrypt the
     * output of rand() to get less predictability, since rand() is
     * often poorly implemented.
     */
    if (++calls == 1) {
        srand((unsigned) (time(NULL) ^ ZCR_SEED2));
    }

    unsigned long temp_keys[CRYPT_KEY_COUNT];
    memcpy(temp_keys, pkeys, sizeof(temp_keys));

    for (n = 0; n < RAND_HEAD_LEN - 2; n++) {
        c = (rand() >> 7) & 0xff;
        header[n] = (unsigned char) zencode(temp_keys, pcrc_32_tab, c, t);
    }
    /* Encrypt random header (last two bytes is high word of crc) */
    for (n = 0; n < RAND_HEAD_LEN - 2; n++) {
        buf[n] = (unsigned char) zencode(pkeys, pcrc_32_tab, header[n], t);
    }
    buf[n++] = (unsigned char) zencode(
        pkeys, pcrc_32_tab, (int) (crcForCrypting >> 16) & 0xff, t);
    buf[n++] = (unsigned char) zencode(
        pkeys, pcrc_32_tab, (int) (crcForCrypting >> 24) & 0xff, t);
}

inline void crypthead(const char *passwd, unsigned char *buf,
    unsigned long *pkeys, const z_crc_t FAR *pcrc_32_tab,
    unsigned long crcForCrypting)
{
    init_keys(passwd, pkeys, pcrc_32_tab);
    crypthead_keys(buf, pkeys, pcrc_32_tab, crcForCrypting);
}
