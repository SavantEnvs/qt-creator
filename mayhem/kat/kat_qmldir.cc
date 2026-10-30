// Known-answer probe for QmlDirParser (src/libs/qmljs/parser/qmldirparser.cpp). Built with the
// project's NORMAL flags by mayhem/build.sh and driven by mayhem/test.sh.
#include <qmljs/parser/qmldirparser_p.h>

#include <QString>
#include <QStringList>

#include <cstdio>

int main()
{
    QmlDirParser p;
    const bool ok = p.parse(QStringLiteral("module My.Module\n"
                                           "plugin myplugin\n"
                                           "Button 1.0 Button.qml\n"
                                           "singleton Theme 2.1 Theme.qml\n"
                                           "typeinfo plugins.qmltypes\n"
                                           "depends QtQuick 2.0\n"));
    std::printf("parse_failed=%d hasError=%d\n", ok, p.hasError());
    std::printf("namespace=%s\n", p.typeNamespace().toUtf8().constData());
    std::printf("plugins=%d first=%s\n", int(p.plugins().size()),
                p.plugins().isEmpty() ? "" : p.plugins().first().name.toUtf8().constData());
    std::printf("components=%d\n", int(p.components().size()));
    // components() is a QMultiHash: iteration order changes per process, so sort before printing.
    QStringList lines;
    for (const auto &c : p.components())
        lines << QStringLiteral("component %1 %2.%3 %4 singleton=%5")
                     .arg(c.typeName).arg(c.majorVersion).arg(c.minorVersion).arg(c.fileName).arg(c.singleton);
    lines.sort();
    for (const QString &l : lines)
        std::printf("%s\n", l.toUtf8().constData());
    std::printf("typeinfos=%d dependencies=%d\n", int(p.typeInfos().size()),
                int(p.dependencies().size()));

    QmlDirParser bad;
    const bool badOk = bad.parse(QStringLiteral("module\nFoo 1 Foo.qml\n"));
    std::printf("bad.parse_failed=%d bad.hasError=%d\n", badOk, bad.hasError());
    return 0;
}
