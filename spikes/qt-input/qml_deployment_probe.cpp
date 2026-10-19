#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QDebug>
#include <cstdio>
#include <memory>

int main(int argc,char **argv)
{
    QGuiApplication app(argc,argv);
    if (argc!=2) return 2;
    QQuickStyle::setStyle("Basic");
    QQmlEngine engine;
    // Exercise copied filesystem modules and their embedded QML resources,
    // without Qt's original installation directory as an import fallback.
    engine.setImportPathList({QString::fromLocal8Bit(argv[1]),
                              QStringLiteral("qrc:/qt-project.org/imports"),
                              QStringLiteral("qrc:/qt/qml")});
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        import QtQuick.Controls
        import QtQuick.Layouts
        import QtQuick.Dialogs as NativeDialogs
        Item {
            width: 640; height: 480
            RowLayout {
                TextField { text: "copied module" }
                Button { text: "Ready" }
                ComboBox { model: ["one", "two"] }
                CheckBox { checked: true }
            }
            NativeDialogs.FolderDialog {}
            NativeDialogs.FileDialog {}
        }
    )",QUrl(QStringLiteral("file:///workbench-qml-deployment-proof.qml")));
    std::unique_ptr<QObject> root(component.create());
    if (!root) {
        qCritical() << component.errors();
        return 1;
    }
    std::puts("PASS: copied QML closure instantiates Basic controls, layouts and dialog components without installation import fallback");
    return 0;
}
