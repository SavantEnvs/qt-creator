// Fuzzes QmlDirParser (src/libs/qmljs/parser/qmldirparser.cpp): the `qmldir` module-definition file
// parser (module/import/plugin/typeinfo/singleton/... directives).
#include <qmljs/parser/qmldirparser_p.h>

#include <QString>

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    const QString source = QString::fromUtf8(reinterpret_cast<const char *>(data),
                                             static_cast<qsizetype>(size));
    QmlDirParser parser;
    parser.parse(source);
    (void) parser.hasError();
    (void) parser.errors(QStringLiteral("fuzz"));
    (void) parser.components();
    (void) parser.dependencies();
    (void) parser.imports();
    (void) parser.scripts();
    (void) parser.plugins();
    (void) parser.typeInfos();
    (void) parser.classNames();
    return 0;
}
