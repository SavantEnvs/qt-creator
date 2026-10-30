// Known-answer probe for the GLSL parser (src/libs/glsl). Built with the project's NORMAL flags by
// mayhem/build.sh and driven by mayhem/test.sh, which compares stdout against exact values.
#include <glsl/glslast.h>
#include <glsl/glslastdump.h>
#include <glsl/glslengine.h>
#include <glsl/glsllexer.h>
#include <glsl/glslparser.h>

#include <QByteArray>
#include <QIODevice>
#include <QString>
#include <QTextStream>

#include <cstdio>

using namespace GLSL;

static void run(const char *label, const QByteArray &src, bool expression)
{
    Engine engine;
    Parser parser(&engine, src.constData(), src.size(), Lexer::Variant_All);
    AST *ast = expression ? static_cast<AST *>(parser.parseExpression())
                          : static_cast<AST *>(parser.parse());
    std::printf("== %s parsed=%d\n", label, ast != nullptr);
    if (!ast)
        return;
    QString out;
    QTextStream stream(&out, QIODevice::WriteOnly);
    ASTDump dump(stream);
    dump(ast);
    stream.flush();
    std::fputs(out.toUtf8().constData(), stdout);
}

int main()
{
    run("function", "void main() { gl_FragColor = vec4(1.0); }", false);
    run("expression", "a + b * 2", true);
    run("broken", "void main( {", false);
    return 0;
}
