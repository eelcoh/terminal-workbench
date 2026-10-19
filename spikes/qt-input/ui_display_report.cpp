#include "ui_display_report.h"
#include "terminal_item.h"
#include <QQuickWindow>
#include <QGuiApplication>
#include <QSGRendererInterface>
#include <QScreen>
#include <QEvent>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QFileInfo>
#include <QDir>
#include <QJSValue>
#include <QDateTime>

UiDisplayReport::UiDisplayReport(QQuickWindow *window,QString path):QObject(window),window_(window),path_(std::move(path)) {
    timer_.setSingleShot(true);timer_.setInterval(200);
    connect(&timer_,&QTimer::timeout,this,&UiDisplayReport::write);
    window_->installEventFilter(this);
    connect(window_,&QWindow::screenChanged,this,&UiDisplayReport::schedule);
    // QML-owned list; notifications discover new items without polling.
    connect(window_,SIGNAL(terminalItemsChanged()),this,SLOT(schedule()));
    schedule();
}
void UiDisplayReport::schedule(){timer_.start();}
bool UiDisplayReport::eventFilter(QObject *object,QEvent *event) {
    switch(event->type()) {
    case QEvent::Resize:case QEvent::Move:case QEvent::Expose:
    case QEvent::ScreenChangeInternal:case QEvent::DevicePixelRatioChange:schedule();break;
    default:break;
    }
    return QObject::eventFilter(object,event);
}
void UiDisplayReport::write() {
    QJsonArray panes;
    const auto value=window_->property("terminalItems");
    const auto items=value.metaType()==QMetaType::fromType<QJSValue>()?value.value<QJSValue>().toVariant().toList():value.toList();
    for(const auto &entry:items) {
        auto *item=qobject_cast<TerminalItem *>(entry.value<QObject *>());if(!item)continue;
        if(!watched_.contains(item)) {
            watched_.insert(item);
            connect(item,&TerminalItem::textFontChanged,this,&UiDisplayReport::schedule);
            connect(item,&TerminalItem::statusChanged,this,&UiDisplayReport::schedule);
            connect(item,&TerminalItem::presentationEnabledChanged,this,&UiDisplayReport::schedule);
            connect(item,&QObject::destroyed,this,[this,item]{watched_.remove(item);schedule();});
        }
        panes.append(QJsonObject::fromVariantMap(item->renderDiagnostics()));
    }
    QJsonObject screen;
    if(auto *display=window_->screen()) {
        if(!watched_.contains(display)) {
            auto *source=display;watched_.insert(source);
            connect(source,&QScreen::geometryChanged,this,&UiDisplayReport::schedule);
            connect(source,&QScreen::availableGeometryChanged,this,&UiDisplayReport::schedule);
            connect(source,&QScreen::logicalDotsPerInchChanged,this,&UiDisplayReport::schedule);
            connect(source,&QScreen::physicalDotsPerInchChanged,this,&UiDisplayReport::schedule);
            connect(source,&QObject::destroyed,this,[this,source]{watched_.remove(source);schedule();});
        }
        const auto geometry=display->geometry(),available=display->availableGeometry();
        screen={{"name",display->name()},{"logical_dpi",display->logicalDotsPerInch()},
            {"physical_dpi",display->physicalDotsPerInch()},{"dpr",display->devicePixelRatio()},
            {"geometry",QJsonArray{geometry.x(),geometry.y(),geometry.width(),geometry.height()}},
            {"available_geometry",QJsonArray{available.x(),available.y(),available.width(),available.height()}}};
    }
    const auto api=window_->rendererInterface()->graphicsApi();
    const char *backend="other";
    switch(api) {
    case QSGRendererInterface::Software:backend="software";break;
    case QSGRendererInterface::OpenGL:backend="OpenGL";break;
    case QSGRendererInterface::Metal:backend="Metal";break;
    case QSGRendererInterface::Vulkan:backend="Vulkan";break;
    case QSGRendererInterface::Null:backend="null";break;
    default:break;
    }
    const auto dpr=window_->devicePixelRatio();
    const QJsonObject report{{"schema",1},{"captured_utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"qt_version",qVersion()},{"platform",QGuiApplication::platformName()},
        {"graphics_api",backend},{"render_path","QPainter row images -> QQuickPaintedItem -> Qt Quick"},
        {"window_dpr",dpr},{"window_logical_size",QJsonArray{window_->width(),window_->height()}},
        {"expected_backing_pixels",QJsonArray{qRound(window_->width()*dpr),qRound(window_->height()*dpr)}},
        {"overview",window_->property("overview").toBool()},{"screen",screen},{"panes",panes}};
    if(!QDir().mkpath(QFileInfo(path_).absolutePath())) {qWarning("Display report: cannot create output directory");return;}
    QSaveFile file(path_);
    if(!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(report).toJson())<0 || !file.commit())
        qWarning("Display report: cannot write output file");
}
