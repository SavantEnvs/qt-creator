// Fuzzes FuzzyMatcher (src/libs/utils/fuzzymatcher.cpp): the camel-hump/wildcard pattern -> regexp
// compiler that backs Qt Creator's locator, plus matching and highlight extraction.
//
// Input: byte 0 selects the case-sensitivity mode and multi-word flag, the rest is UTF-8 text split at
// the first '\n' into <pattern>\n<candidate>.
#include <utils/fuzzymatcher.h>

#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QString>

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 2)
        return -1;
    const uint8_t flags = data[0];
    const QString text = QString::fromUtf8(reinterpret_cast<const char *>(data + 1),
                                           static_cast<qsizetype>(size - 1));
    const qsizetype nl = text.indexOf(QLatin1Char('\n'));
    const QString pattern = nl < 0 ? text : text.left(nl);
    const QString candidate = nl < 0 ? QString() : text.mid(nl + 1);
    const bool multiWord = flags & 0x04;

    QRegularExpression re;
    switch (flags & 0x03) {
    case 0:
        re = FuzzyMatcher::createRegExp(pattern, FuzzyMatcher::CaseSensitivity::CaseInsensitive, multiWord);
        break;
    case 1:
        re = FuzzyMatcher::createRegExp(pattern, FuzzyMatcher::CaseSensitivity::CaseSensitive, multiWord);
        break;
    case 2:
        re = FuzzyMatcher::createRegExp(pattern,
                                        FuzzyMatcher::CaseSensitivity::FirstLetterCaseSensitive,
                                        multiWord);
        break;
    default:
        re = FuzzyMatcher::createRegExp(pattern, Qt::CaseInsensitive, multiWord);
        break;
    }

    const QRegularExpressionMatch match = re.match(candidate);
    if (match.hasMatch())
        (void) FuzzyMatcher::highlightingPositions(match);
    return 0;
}
