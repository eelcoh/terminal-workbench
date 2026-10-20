#include "ui_startup_check.h"
#include "deployment_check.h"
#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QJSValue>
#include <QJsonArray>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

QJsonObject clientCheckConfiguration(const QString &directory)
{
    const QDir root(directory);
    return {{"state",root.filePath("unused-session")},{"cwd",directory},
        {"catalog",QJsonObject{{"workspaces",QJsonArray{}}}},
        {"saved_session",false},{"catalog_updates",false},
        {"preferences",root.filePath("config.conf")},{"font_overrides",root.filePath("font.json")},
        {"shortcut_overrides",root.filePath("shortcuts.conf")},
        {"theme_store",root.filePath("imports")},{"theme_directory",root.filePath("themes")}};
}

QJsonObject completeClientCheck(QJsonObject modules, QQmlApplicationEngine &engine, const QStringList &warnings)
{
    // Settle the same queued initialization used by normal launches. No main
    // event loop, user configuration, catalog watcher or terminal is needed.
    for(int i=0;i<3;++i)QCoreApplication::processEvents();
    QStringList errors=warnings;
    auto *window=engine.rootObjects().isEmpty()?nullptr:qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    const auto count=[window](const char *property){
        if(!window)return -1;
        const auto value=window->property(property);
        if(!value.isValid())return -1;
        return int((value.metaType()==QMetaType::fromType<QJSValue>()?value.value<QJSValue>().toVariant():value).toList().size());
    };
    int visible=0;for(const auto *candidate:QGuiApplication::allWindows())if(candidate->isVisible())++visible;
    if(!window)errors.append("Workbench root window was not created");
    else {
        if(!window->property("viewReady").toBool())errors.append("Workbench queued initialization did not finish");
        if(count("spaceIds")!=0 || count("terminalItems")!=0)errors.append("Client check created unexpected workspace or terminal items");
        if(!window->findChild<QObject *>("aboutDialog"))errors.append("Workbench About component was not created");
    }
    if(visible)errors.append("Client check exposed a window");
    modules["client_ok"]=errors.isEmpty();
    modules["client"]=QJsonObject{{"workspace_count",count("spaceIds")},{"terminal_count",count("terminalItems")},
        {"visible_window_count",visible},{"errors",QJsonArray::fromStringList(errors)}};
    modules["qml_ok"]=modules["qml_ok"].toBool() && errors.isEmpty();
    auto images=modules["loaded_images"].toArray();
    for(const auto &path:deploymentLoadedImages())if(!images.contains(path))images.append(path);
    modules["loaded_images"]=images;
    modules["import_paths"]=QJsonArray::fromStringList(engine.importPathList());
    return modules;
}
