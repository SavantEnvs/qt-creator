// Fuzzes the QML/JS front end Qt Creator ships in src/libs/qmljs/parser: lexer, parser (all five
// start symbols) and the AST visitor traversal, as used by the QML editor for every open document.
//
// Input: byte 0 selects the start symbol / lexer mode, the rest is UTF-8 source text.
#include <qmljs/parser/qmljsast_p.h>
#include <qmljs/parser/qmljsastvisitor_p.h>
#include <qmljs/parser/qmljsengine_p.h>
#include <qmljs/parser/qmljslexer_p.h>
#include <qmljs/parser/qmljsparser_p.h>

#include <QString>

#include <cstddef>
#include <cstdint>

using namespace QmlJS;

namespace {
class NullVisitor : public AST::Visitor
{
public:
    bool preVisit(AST::Node *) override { return true; }
    void throwRecursionDepthError() override {}
};
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1)
        return -1;
    const uint8_t mode = data[0] % 7;
    const QString source = QString::fromUtf8(reinterpret_cast<const char *>(data + 1),
                                             static_cast<qsizetype>(size - 1));

    Engine engine;
    Lexer lexer(&engine);
    const bool qmlMode = mode <= 2;
    lexer.setCode(source, /*line=*/1, qmlMode);

    if (mode == 6) {
        // Lexer only: drain tokens.
        while (lexer.lex() != 0 /* EOF_SYMBOL */) {
        }
        return 0;
    }

    Parser parser(&engine);
    switch (mode) {
    case 0: parser.parse(); break;
    case 1: parser.parseUiObjectMember(); break;
    case 2: parser.parse(); break;
    case 3: parser.parseProgram(); break;
    case 4: parser.parseModule(); break;
    default: parser.parseExpression(); break;
    }

    (void) parser.diagnosticMessages();
    if (AST::Node *root = parser.rootNode()) {
        NullVisitor visitor;
        root->accept(&visitor);
    }
    return 0;
}
