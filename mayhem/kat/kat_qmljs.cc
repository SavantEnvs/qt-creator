// Known-answer probe for the QML/JS parser (src/libs/qmljs/parser). Built with the project's NORMAL
// flags by mayhem/build.sh and driven by mayhem/test.sh, which compares stdout against exact values.
#include <qmljs/parser/qmljsast_p.h>
#include <qmljs/parser/qmljsastvisitor_p.h>
#include <qmljs/parser/qmljsengine_p.h>
#include <qmljs/parser/qmljslexer_p.h>
#include <qmljs/parser/qmljsparser_p.h>

#include <QString>

#include <cstdio>

using namespace QmlJS;

namespace {
class PrintVisitor : public AST::Visitor
{
public:
    void throwRecursionDepthError() override {}
    bool visit(AST::UiObjectDefinition *ast) override
    {
        std::printf("object %s\n", ast->qualifiedTypeNameId->name.toUtf8().constData());
        return true;
    }
    bool visit(AST::UiScriptBinding *ast) override
    {
        std::printf("binding %s\n", ast->qualifiedId->name.toUtf8().constData());
        return true;
    }
    bool visit(AST::NumericLiteral *ast) override
    {
        std::printf("number %g\n", ast->value);
        return true;
    }
    bool visit(AST::StringLiteral *ast) override
    {
        std::printf("string %s\n", ast->value.toUtf8().constData());
        return true;
    }
    bool visit(AST::BinaryExpression *ast) override
    {
        std::printf("binary op=%d\n", ast->op);
        return true;
    }
};

bool run(const char *label, const QString &source, int mode)
{
    Engine engine;
    Lexer lexer(&engine);
    lexer.setCode(source, 1, mode == 0);
    Parser parser(&engine);
    const bool ok = mode == 0 ? parser.parse() : parser.parseExpression();
    std::printf("== %s ok=%d diagnostics=%d\n", label, ok, int(parser.diagnosticMessages().size()));
    if (AST::Node *root = parser.rootNode()) {
        PrintVisitor v;
        root->accept(&v);
    }
    return ok;
}
} // namespace

int main()
{
    run("qml", QStringLiteral("import QtQuick 2.15\n"
                              "Rectangle {\n  id: root\n  width: 100\n  Text { text: \"hi\" }\n}\n"), 0);
    run("broken", QStringLiteral("Rectangle {\n  width: \n"), 0);
    run("expr", QStringLiteral("1 + 2 * 3"), 1);
    return 0;
}
