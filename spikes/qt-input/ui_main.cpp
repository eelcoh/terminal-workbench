// Isolated UI slice; the launcher owns disposable runtime lifecycle.
#include "terminal_item.h"
#include "ui_session.h"
#include <QQmlContext>
#include <QJSValue>
#include "terminal_accessibility.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QQmlError>
#include <cstdio>

static QVariantList listProperty(QObject *object,const char *name) {
    const auto value=object->property(name);
    return value.metaType()==QMetaType::fromType<QJSValue>() ? value.value<QJSValue>().toVariant().toList() : value.toList();
}
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    app.setApplicationName("Terminal Workbench UI Study");
    app.setDesktopFileName("terminal-workbench-ui-study");
    QQuickStyle::setStyle("Basic");
    const auto args=app.arguments();
    if(argc<2)return 2;
    QFile config(QString::fromLocal8Bit(argv[1]));if(!config.open(QIODevice::ReadOnly))return 2;
    const auto configuration=QJsonDocument::fromJson(config.readAll()).object();
    if(!configuration["catalog"].isObject() || configuration["state"].toString().isEmpty())return 2;
    UiSession session(configuration);
    qmlRegisterType<TerminalItem>("Workbench.Probe",1,0,"TerminalItem");installTerminalAccessibility();
    QQmlApplicationEngine engine;
    QObject::connect(&engine,&QQmlEngine::warnings,&app,[](const QList<QQmlError> &errors){for(const auto &error:errors)std::fprintf(stderr,"Qt UI: %s\n",qPrintable(error.toString()));});
    engine.rootContext()->setContextProperty("testFolderDialogs",args.contains("--test-report"));
    engine.rootContext()->setContextProperty("studySession",&session);engine.load(QUrl("qrc:/Ui.qml"));if(engine.rootObjects().isEmpty()){std::fputs("Qt UI: no root window was created\n",stderr);return 1;}
    auto *window=qobject_cast<QQuickWindow *>(engine.rootObjects().first());if(!window){std::fputs("Qt UI: root object is not a window\n",stderr);return 1;}
    // Explicit test-only snapshots; no user terminal content is recorded by default.
    const int reportIndex=args.indexOf("--test-report");QTimer reporter;
    if(reportIndex>=0 && reportIndex+1<args.size()) {
        const auto path=args[reportIndex+1];reporter.setInterval(80);
        QObject::connect(&reporter,&QTimer::timeout,&app,[&,path] {
            QJsonArray panes;
            const auto items=listProperty(window,"terminalItems");
            for(const auto &value:items) {
                auto *item=qobject_cast<TerminalItem *>(value.value<QObject *>());if(!item)continue;
                const auto size=item->terminalGridSize();const auto position=item->mapToScene(QPointF(item->width()/2,item->height()/2));
                const auto cell=item->cellSize();
                const auto bounds=item->parentItem()->mapRectToScene(item->parentItem()->boundingRect());
                const QSize geometry(qBound(1,int((item->width()-16)/cell.width()),1000),qBound(1,int((item->height()-16)/cell.height()),1000));
                panes.append(QJsonObject{{"ready",item->ready()},{"text",item->viewportText()},
                    {"cursor",item->cursorPresentation()},
                    {"selection",item->accessibleSnapshot().selectionStart>=0},
                    {"identity",item->property("stableIdentity").toString()},
                    {"workspace",item->property("workspaceIndex").toInt()},{"ordinal",item->property("orderInWorkspace").toInt()},
                    {"theme",item->themeIndex()},{"background",item->backgroundColor().name()},
                    {"sample_green",item->cellForeground(1,2).name()},{"sample_red",item->cellForeground(1,0).name()},{"sample_rgb",item->cellForeground(1,1).name()},
                    {"columns",size.width()},{"rows",size.height()},{"geometry_ready",size==geometry},{"focus",item->hasActiveFocus()},
                    {"left",bounds.left()},{"top",bounds.top()},{"right",bounds.right()},{"bottom",bounds.bottom()},
                    {"x",position.x()},{"y",position.y()},{"rasterized_rows",double(item->rasterizedRows())}});
            }
            const auto *createButton=window->findChild<QQuickItem *>("newWorkspaceButton");
            const auto createPoint=createButton->mapToScene(QPointF(createButton->width()/2,createButton->height()/2));
            const auto center=[window](const char *name){const auto *item=window->findChild<QQuickItem *>(name);const auto point=item->mapToScene(QPointF(item->width()/2,item->height()/2));return QJsonArray{point.x(),point.y()};};
            QJsonArray folderChoices;QJsonArray folderAccept;QJsonArray folderList;bool folderMoving=false;
            // Observe the installed fallback controls; no test actions enter production QML.
            QList<QQuickItem *> pending{window->contentItem()};
            while(!pending.isEmpty()) {
                auto *item=pending.takeLast();
                if(!item->isVisible())continue;
                pending.append(item->childItems());
                const auto point=item->mapToScene(QPointF(item->width()/2,item->height()/2));
                bool unclipped=true;
                for(auto *parent=item->parentItem();parent;parent=parent->parentItem())
                    if(parent->clip() && !parent->mapRectToScene(parent->boundingRect()).contains(point)) {unclipped=false;break;}
                if(item->objectName()=="folderDialogListView") {folderList=QJsonArray{point.x(),point.y()};folderMoving=item->property("moving").toBool();}
                if(unclipped && item->objectName().startsWith("folderDialogDelegate"))folderChoices.append(QJsonObject{{"name",item->property("fileName").toString()},{"selected",item->property("highlighted").toBool()},{"point",QJsonArray{point.x(),point.y()}}});
                if(item->property("text").toString()=="Choose folder" && item->inherits("QQuickAbstractButton"))folderAccept=QJsonArray{point.x(),point.y()};
            }
            const QJsonObject data{{"workspace",window->property("workspace").toInt()},
                {"selected",window->property("selected").toInt()},{"overview",window->property("overview").toBool()},
                {"drop_allowed",window->property("dropAllowed").toBool()},
                {"drag_slot",window->property("dragSlot").toInt()},{"drop_space",window->property("dropSpace").toInt()},{"drop_position",window->property("dropPosition").toInt()},
                {"content_y",window->findChild<QQuickItem *>("deck")->property("contentY").toDouble()},
                {"content_x",window->findChild<QQuickItem *>("deck")->property("contentX").toDouble()},
                {"busy",session.busy()},{"error",session.error()},
                {"workspace_count",listProperty(window,"spaces").size()},
                {"appearance_open",window->property("appearanceOpen").toBool()},
                {"application_menu_open",window->property("applicationMenuOpen").toBool()},
                {"theme_search_focus",window->property("themeSearchFocus").toBool()},
                {"theme_matches",window->property("themeMatchCount").toInt()},
                {"chrome_background",window->color().name()},
                {"application_menu_button",center("applicationMenuButton")},{"appearance_menu_item",center("appearanceMenuItem")},
                {"rename_workspace_item",center("renameWorkspaceMenuItem")},{"workspace_folder_item",center("workspaceFolderMenuItem")},
                {"workspace_form_mode",window->property("workspaceFormMode").toString()},
                {"workspace_names",QJsonArray::fromVariantList(listProperty(window,"spaces"))},
                {"workspace_directories",QJsonArray::fromVariantList(listProperty(window,"directories"))},
                {"folder_focus",window->property("folderFocused").toBool()},
                {"folder_browser_open",window->property("folderBrowserOpen").toBool()},
                {"folder_browser_location",window->property("folderBrowserLocation").toUrl().toLocalFile()},
                {"folder_choices",folderChoices},{"folder_accept",folderAccept},{"folder_list",folderList},{"folder_list_moving",folderMoving},
                {"folder_browser_selection",window->property("folderBrowserSelection").toUrl().toLocalFile()},
                {"browse_folder_button",center("browseWorkspaceFolderButton")},
                {"name_input",window->findChild<QQuickItem *>("workspaceNameInput")->property("text").toString()},
                {"folder_input",window->findChild<QQuickItem *>("workspaceFolderInput")->property("text").toString()},
                {"form_submit_enabled",window->findChild<QQuickItem *>("submitWorkspaceButton")->isEnabled()},
                {"creation_open",window->property("creationOpen").toBool()},
                {"drawer_open",window->property("drawerOpen").toBool()},{"name_focus",window->property("nameFocused").toBool()},
                {"new_workspace_button",QJsonArray{createPoint.x(),createPoint.y()}},
                {"panes",panes},{"platform",QGuiApplication::platformName()}};
            QSaveFile report(path);if(report.open(QIODevice::WriteOnly)){report.write(QJsonDocument(data).toJson());report.commit();}
            if(QFile::exists(path+".capture")) {
                const auto image=window->grabWindow();
                if(image.save(path+".png")) {
                    QJsonArray cursors;
                    for(const auto &value:items) {
                        auto *item=qobject_cast<TerminalItem *>(value.value<QObject *>());if(!item)continue;
                        const auto rect=item->mapRectToScene(item->caret());
                        const qreal sx=qreal(image.width())/window->width(),sy=qreal(image.height())/window->height();
                        const QRect pixels=QRect(qRound(rect.x()*sx),qRound(rect.y()*sy),qRound(rect.width()*sx),qRound(rect.height()*sy)).intersected(image.rect());
                        int cursor=0,background=0;
                        for(int y=pixels.top();y<=pixels.bottom();++y)for(int x=pixels.left();x<=pixels.right();++x) {
                            const auto color=image.pixelColor(x,y);
                            if(color==item->cursorColor())++cursor;
                            if(color==item->backgroundColor())++background;
                        }
                        cursors.append(QJsonObject{{"cursor_pixels",cursor},{"background_pixels",background},{"area",pixels.width()*pixels.height()}});
                    }
                    QSaveFile metrics(path+".cursor.json");if(metrics.open(QIODevice::WriteOnly)){metrics.write(QJsonDocument(cursors).toJson());metrics.commit();}
                    QFile::remove(path+".capture");
                }
            }
        });reporter.start();
    }
    return app.exec();
}
