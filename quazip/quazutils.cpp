#include "quazutils.h"

#include <QString>

bool QuaZUtils::isAscii(const QString &text)
{
    for (const QChar &ch : text) {
        if (ch.unicode() > 127)
            return false;
    }

    return true;
}
