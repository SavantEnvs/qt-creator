// Fuzzes the GLSL lexer + parser in src/libs/glsl (the grammar-table driven parser behind the GLSL
// editor), for a translation unit and for a single expression, across all language variants, and
// walks the resulting AST with the library's own dumper.
//
// Input: byte 0 selects the language variant bits and the start symbol, the rest is the source.
#include <glsl/glslast.h>
#include <glsl/glslastdump.h>
#include <glsl/glslengine.h>
#include <glsl/glsllexer.h>
#include <glsl/glslparser.h>

#include <QByteArray>
#include <QIODevice>
#include <QString>
#include <QTextStream>

#include <cstddef>
#include <cstdint>

using namespace GLSL;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 2)
        return -1;
    const uint8_t sel = data[0];
    int variant = 0;
    if (sel & 0x01) variant |= Lexer::Variant_GLSL_120;
    if (sel & 0x02) variant |= Lexer::Variant_GLSL_150;
    if (sel & 0x04) variant |= Lexer::Variant_GLSL_400 | Lexer::Variant_GLSL_460;
    if (sel & 0x08) variant |= Lexer::Variant_GLSL_ES_100;
    if (sel & 0x10) variant |= Lexer::Variant_VertexShader;
    if (sel & 0x20) variant |= Lexer::Variant_FragmentShader;
    if (sel & 0x40) variant |= Lexer::Variant_Vulkan;
    const bool expression = sel & 0x80;

    Engine engine;
    // The GLSL lexer peeks one byte past the end of its input; Qt Creator always hands it a
    // NUL-terminated QByteArray, so give it the same contract.
    const QByteArray source(reinterpret_cast<const char *>(data + 1), static_cast<qsizetype>(size - 1));
    Parser parser(&engine, source.constData(), static_cast<unsigned>(source.size()), variant);
    AST *ast = expression ? static_cast<AST *>(parser.parseExpression())
                          : static_cast<AST *>(parser.parse());
    if (ast) {
        QString out;
        QTextStream stream(&out, QIODevice::WriteOnly);
        ASTDump dump(stream);
        dump(ast);
    }
    return 0;
}
