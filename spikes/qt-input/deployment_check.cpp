#include "deployment_check.h"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QJsonArray>
#include <QQmlComponent>
#include <QQmlEngine>
#include <memory>

QJsonObject deploymentCheck(const QString &qmlRoot)
{
    QQmlEngine engine;
    if (!qmlRoot.isEmpty())
        engine.setImportPathList({qmlRoot, QStringLiteral("qrc:/qt-project.org/imports"), QStringLiteral("qrc:/qt/qml")});
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
    QJsonArray errors;
    for (const auto &error : component.errors()) errors.append(error.toString());
    return {{"schema", 1}, {"qml_ok", bool(root)}, {"qml_errors", errors},
            {"qt_version", QString::fromLatin1(qVersion())},
            {"platform", QGuiApplication::platformName()},
            {"import_paths", QJsonArray::fromStringList(engine.importPathList())},
            {"plugin_paths", QJsonArray::fromStringList(QCoreApplication::libraryPaths())},
            {"loaded_images", QJsonArray::fromStringList(deploymentLoadedImages())}};
}
