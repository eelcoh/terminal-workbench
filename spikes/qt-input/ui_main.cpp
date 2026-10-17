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

static QVariantList listProperty(QObject *object,const char *name) {
    const auto value=object->property(name);
    return value.metaType()==QMetaType::fromType<QJSValue>() ? value.value<QJSValue>().toVariant().toList() : value.toList();
}
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    app.setApplicationName("Terminal Workbench UI Study");
    app.setDesktopFileName("terminal-workbench-ui-study");
    QQuickStyle::setStyle("Basic");
    if(argc<2)return 2;
    QFile config(QString::fromLocal8Bit(argv[1]));if(!config.open(QIODevice::ReadOnly))return 2;
    const auto configuration=QJsonDocument::fromJson(config.readAll()).object();
    if(!configuration["catalog"].isObject() || configuration["state"].toString().isEmpty())return 2;
    UiSession session(configuration);
    qmlRegisterType<TerminalItem>("Workbench.Probe",1,0,"TerminalItem");installTerminalAccessibility();
    QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("studySession",&session);engine.load(QUrl("qrc:/Ui.qml"));if(engine.rootObjects().isEmpty())return 1;
    auto *window=qobject_cast<QQuickWindow *>(engine.rootObjects().first());if(!window)return 1;
    // Explicit test-only snapshots; no user terminal content is recorded by default.
    const auto args=app.arguments();const int reportIndex=args.indexOf("--test-report");QTimer reporter;
    if(reportIndex>=0 && reportIndex+1<args.size()) {
        const auto path=args[reportIndex+1];reporter.setInterval(80);
        QObject::connect(&reporter,&QTimer::timeout,&app,[&,path] {
            QJsonArray panes;
            const auto items=listProperty(window,"terminalItems");
            for(const auto &value:items) {
                auto *item=qobject_cast<TerminalItem *>(value.value<QObject *>());if(!item)continue;
                const auto size=item->terminalGridSize();const auto position=item->mapToScene(QPointF(item->width()/2,item->height()/2));
                const auto cell=item->cellSize();
                const QSize geometry(qBound(1,int((item->width()-16)/cell.width()),1000),qBound(1,int((item->height()-16)/cell.height()),1000));
                panes.append(QJsonObject{{"ready",item->ready()},{"text",item->viewportText()},
                    {"columns",size.width()},{"rows",size.height()},{"geometry_ready",size==geometry},{"focus",item->hasActiveFocus()},
                    {"x",position.x()},{"y",position.y()},{"rasterized_rows",double(item->rasterizedRows())}});
            }
            const auto *createButton=window->findChild<QQuickItem *>("newWorkspaceButton");
            const auto createPoint=createButton->mapToScene(QPointF(createButton->width()/2,createButton->height()/2));
            const QJsonObject data{{"workspace",window->property("workspace").toInt()},
                {"selected",window->property("selected").toInt()},{"overview",window->property("overview").toBool()},
                {"content_x",window->findChild<QQuickItem *>("deck")->property("contentX").toDouble()},
                {"busy",session.busy()},{"error",session.error()},
                {"workspace_count",listProperty(window,"spaces").size()},
                {"creation_open",window->property("creationOpen").toBool()},
                {"drawer_open",window->property("drawerOpen").toBool()},{"name_focus",window->property("nameFocused").toBool()},
                {"new_workspace_button",QJsonArray{createPoint.x(),createPoint.y()}},
                {"panes",panes},{"platform",QGuiApplication::platformName()}};
            QSaveFile report(path);if(report.open(QIODevice::WriteOnly)){report.write(QJsonDocument(data).toJson());report.commit();}
            if(QFile::exists(path+".capture") && window->grabWindow().save(path+".png"))QFile::remove(path+".capture");
        });reporter.start();
    }
    return app.exec();
}
