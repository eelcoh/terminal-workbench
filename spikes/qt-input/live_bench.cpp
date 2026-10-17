// Isolated real-runtime client benchmark. Owns presentation only, never PTYs.
#include "terminal_item.h"
#include "terminal_accessibility.h"
#include <QGuiApplication>
#include <QQuickWindow>
#include <QLocalServer>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QTimer>
#include <QSGRendererInterface>
#include <atomic>
#include <chrono>
#include <ctime>
#include <cmath>
#include <cstdio>

namespace {
qint64 nowNs() {return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
QString graphicsApi(QSGRendererInterface::GraphicsApi api) {
    switch(api) {
    case QSGRendererInterface::OpenGL:return "OpenGL";
    case QSGRendererInterface::Metal:return "Metal";
    case QSGRendererInterface::Vulkan:return "Vulkan";
    case QSGRendererInterface::Software:return "software";
    case QSGRendererInterface::Direct3D11:return "Direct3D11";
    case QSGRendererInterface::Direct3D12:return "Direct3D12";
    default:return "unknown";
    }
}
class LiveTerminal final : public TerminalItem {
public:
    using TerminalItem::TerminalItem;
    void paint(QPainter *painter) override {
        const auto start=nowNs();TerminalItem::paint(painter);
        paintNs.fetch_add(nowNs()-start,std::memory_order_relaxed);
        paints.fetch_add(1,std::memory_order_relaxed);
        paintedThrough.store(refreshedBytes(),std::memory_order_relaxed);
    }
    std::atomic<quint64> paints{0},paintedThrough{0};
    std::atomic<qint64> paintNs{0};
};
struct Sample {quint64 bytes=0,refreshed=0,rows=0,paints=0;qint64 paintNs=0,imageBytes=0;};
Sample sample(LiveTerminal *item) {return {item->receivedBytes(),item->refreshedBytes(),item->rasterizedRows(),
    item->paints.load(),item->paintNs.load(),item->rowImageBytes()};}
class Probe {
public:
    QGuiApplication &app;QQuickWindow window;QLocalServer server;QLocalSocket *controller=nullptr;
    QVector<LiveTerminal *> panes;QVector<Sample> baseline;QVector<quint64> wanted;
    QTimer poll;QString pending,scenario;bool recording=false,established=false,finished=false;
    qint64 started=0,deadline=0;std::clock_t cpuStart=0;quint64 swapStart=0;
    std::atomic<quint64> swaps{0};QSize viewport; qreal scale=0;
    Probe(QGuiApplication &application,const QJsonObject &config):app(application) {
        installTerminalAccessibility();window.setTitle("Workbench live-runtime benchmark");
        window.setColor(QColor("#191d24"));window.resize(1120,680);
        const auto controls=config["sessions"].toArray();
        for(const auto &entry:controls) {
            auto *item=new LiveTerminal(window.contentItem());
            const auto cell=item->cellSize();item->setWidth(80*cell.width()+16);item->setHeight(24*cell.height()+16);
            panes.append(item);item->connectSession(entry.toString());
        }
        const int columns=panes.size()==1?1:5,rows=panes.size()==1?1:3;
        const auto layout=[this,columns,rows] {
            if(panes.isEmpty())return;
            const auto pw=panes[0]->width(),ph=panes[0]->height();
            const auto zoom=std::min(qreal(1),std::min(window.width()/(columns*(pw+8)),window.height()/(rows*(ph+8))));
            for(int i=0;i<panes.size();++i) {auto *item=panes[i];item->setTransformOrigin(QQuickItem::TopLeft);
                item->setScale(zoom);item->setPosition(QPointF((i%columns)*(pw+8)*zoom,(i/columns)*(ph+8)*zoom));}
        };
        layout();
        QObject::connect(&window,&QQuickWindow::widthChanged,&app,[this,layout]{if(!established)layout();});
        QObject::connect(&window,&QQuickWindow::heightChanged,&app,[this,layout]{if(!established)layout();});
        QObject::connect(&window,&QQuickWindow::frameSwapped,&app,[this]{swaps.fetch_add(1);},Qt::DirectConnection);
        QObject::connect(&window,&QQuickWindow::sceneGraphError,&app,[this](auto,const QString &message){fail(message);});
        QObject::connect(&app,&QGuiApplication::lastWindowClosed,&app,[this]{if(!finished)fail("benchmark window closed");});
        QObject::connect(&server,&QLocalServer::newConnection,&app,[this]{
            auto *socket=server.nextPendingConnection();
            if(controller) {socket->abort();socket->deleteLater();return;}
            controller=socket;
            QObject::connect(socket,&QLocalSocket::readyRead,&app,[this]{
                if(controller->bytesAvailable()>65536) {fail("oversized benchmark command");return;}
                while(controller->canReadLine()) {
                    QJsonParseError error;const auto document=QJsonDocument::fromJson(controller->readLine(),&error);
                    if(error.error!=QJsonParseError::NoError || !document.isObject()) {fail("invalid benchmark command");return;}
                    command(document.object());
                }
            });
        });
        if(!server.listen(config["control"].toString())) {fail(server.errorString());return;}
        poll.setInterval(10);QObject::connect(&poll,&QTimer::timeout,&app,[this]{tick();});poll.start();window.show();
    }
    ~Probe() {poll.stop();QObject::disconnect(&window,nullptr,&app,nullptr);window.hide();}
    void send(QJsonObject data) {if(controller)controller->write(QJsonDocument(data).toJson(QJsonDocument::Compact)+'\n');}
    void fail(const QString &message) {if(finished)return;finished=true;send({{"error",message}});std::fprintf(stderr,"FAIL: %s\n",qPrintable(message));app.exit(1);}
    bool geometryValid() const {return window.isExposed() && window.size()==viewport && window.devicePixelRatio()==scale;}
    void command(const QJsonObject &data) {
        if(finished)return;
        const auto op=data["op"].toString();
        if(op=="status") {
            bool ready=window.isExposed() && !panes.isEmpty();
            QJsonArray statuses;
            for(auto *item:panes) {statuses.append(item->status());ready=ready && item->ready() && item->terminalGridSize()==QSize(80,24)
                && item->rasterizedRows()>=24 && item->paints.load()>0 && item->receivedBytes()==item->refreshedBytes();}
            if(ready && !established) {established=true;viewport=window.size();scale=window.devicePixelRatio();}
            send({{"ready",ready},{"statuses",statuses},{"qt_version",QString(qVersion())},
                {"platform",QGuiApplication::platformName()},{"graphics_api",graphicsApi(window.rendererInterface()->graphicsApi())},
                {"device_pixel_ratio",window.devicePixelRatio()},{"width",window.width()},{"height",window.height()},
                {"panes",panes.size()},{"columns",80},{"rows",24}});return;
        }
        if(!established || !geometryValid()) {fail("window hidden, resized or DPR changed; keep the benchmark stationary and exposed");return;}
        if(!pending.isEmpty()) {fail("overlapping benchmark commands");return;}
        if(op=="visibility") {
            const int count=data["visible"].toInt(-1);if(count<0 || count>panes.size()) {fail("invalid visibility count");return;}
            for(int i=0;i<panes.size();++i)panes[i]->setVisible(i<count);
            pending="visibility";deadline=nowNs()+100000000;return;
        }
        if(op=="begin") {
            if(recording) {fail("overlapping scenarios");return;}
            baseline.clear();for(auto *item:panes)baseline.append(sample(item));
            scenario=data["scenario"].toString();recording=true;swapStart=swaps.load();cpuStart=std::clock();started=nowNs();
            send({{"begun",true}});return;
        }
        if(op=="finish") {
            const auto bytes=data["bytes"].toArray();
            if(!recording || bytes.size()!=panes.size()) {fail("invalid completion counters");return;}
            wanted.clear();for(const auto &value:bytes) {
                if(value.toDouble()<0) {fail("negative byte counter");return;}
                wanted.append(quint64(value.toDouble()));
            }
            pending="finish";deadline=nowNs()+60000000000LL;return;
        }
        if(op=="reveal") {
            baseline.clear();for(auto *item:panes) {baseline.append(sample(item));item->setVisible(true);}
            pending="reveal";deadline=nowNs()+60000000000LL;return;
        }
        if(op=="verify") {
            const auto markers=data["markers"].toArray();bool matches=markers.size()==panes.size();
            for(int i=0;matches && i<panes.size();++i)matches=panes[i]->viewportText().contains(markers[i].toString());
            send({{"markers_match",matches}});return;
        }
        if(op=="capture") {send({{"captured",window.grabWindow().save(data["path"].toString())}});return;}
        if(op=="quit") {finished=true;send({{"done",true}});if(controller)controller->flush();QTimer::singleShot(100,&app,&QGuiApplication::quit);return;}
        fail("unknown benchmark operation");
    }
    QJsonObject result(bool reveal) {
        QJsonArray details;quint64 bytes=0,rows=0,paints=0;qint64 paintNs=0,imageBytes=0;
        for(int i=0;i<panes.size();++i) {
            const auto current=sample(panes[i]);const auto &before=baseline[i];
            const auto db=current.bytes-before.bytes,dr=current.rows-before.rows,dp=current.paints-before.paints;
            bytes+=db;rows+=dr;paints+=dp;paintNs+=current.paintNs-before.paintNs;imageBytes+=current.imageBytes;
            details.append(QJsonObject{{"received_bytes",double(db)},{"rasterized_rows",double(dr)},{"paints",double(dp)},
                {"visible",panes[i]->isVisible()},{"row_image_bytes",double(current.imageBytes)}});
        }
        QJsonObject data{{"scenario",scenario},{"received_bytes",double(bytes)},{"rasterized_rows",double(rows)},
            {"paints",double(paints)},{"paint_wall_ms",paintNs/1e6},{"row_image_bytes",double(imageBytes)},{"items",details}};
        if(!reveal) {data.insert("wall_seconds",(nowNs()-started)/1e9);data.insert("viewer_cpu_seconds",double(std::clock()-cpuStart)/CLOCKS_PER_SEC);
            data.insert("swap_notifications",double(swaps.load()-swapStart));}
        return data;
    }
    void tick() {
        if(finished || pending.isEmpty())return;
        if(!geometryValid()) {fail("window hidden, resized or DPR changed during sample");return;}
        if(pending=="visibility") {if(nowNs()>=deadline) {pending.clear();send({{"visibility_ready",true}});}return;}
        if(nowNs()>deadline) {fail("terminal output/paint did not catch up");return;}
        for(int i=0;i<panes.size();++i) {
            auto *item=panes[i];if(!item->ready()) {fail(item->status());return;}
            const auto target=pending=="finish"?baseline[i].bytes+wanted[i]:item->receivedBytes();
            if(item->receivedBytes()>target) {fail("unexpected terminal output beyond the producer payload");return;}
            if(item->receivedBytes()<target || item->refreshedBytes()!=target || (item->isVisible() && item->paintedThrough.load()<target))return;
        }
        const bool reveal=pending=="reveal";auto data=result(reveal);
        if(!reveal) {
            for(int i=0;i<panes.size();++i) {
                const auto &before=baseline[i];auto *item=panes[i];
                if(!item->isVisible() && (item->rasterizedRows()!=before.rows || item->paints.load()!=before.paints)) {fail("hidden terminal performed raster/paint work");return;}
                if(scenario=="idle" && (item->receivedBytes()!=before.bytes || item->rasterizedRows()!=before.rows || item->paints.load()!=before.paints)) {fail("idle terminal performed output/raster/paint work");return;}
            }
            recording=false;
        }
        pending.clear();send(data);
    }
};
}
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);app.setApplicationName("Workbench Live Terminal Benchmark");
    app.setDesktopFileName("terminal-workbench-live-benchmark");
    if(argc!=2) {std::fputs("usage: workbench-live-bench CONFIG.json\n",stderr);return 2;}
    QFile file(QString::fromLocal8Bit(argv[1]));if(!file.open(QIODevice::ReadOnly))return 2;
    const auto config=QJsonDocument::fromJson(file.readAll()).object();
    if(config["sessions"].toArray().size()!=1 && config["sessions"].toArray().size()!=15)return 2;
    Probe probe(app,config);if(probe.finished)return 1;return app.exec();
}
