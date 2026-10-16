// Live Qt Quick image/scene probe. Synthetic VT grids; no user sessions or PTYs.
#include "terminal_text.h"
#include "terminal_bench_fixture.h"
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQuickPaintedItem>
#include <QSGRendererInterface>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QPainter>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <ctime>
#include <cstdio>
#include <stdexcept>

namespace {
using Grid=QVector<QVector<TerminalTextCell>>;
qint64 now() {return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
void require(bool ok,const char *message) {if(!ok)throw std::runtime_error(message);}
QString apiName(QSGRendererInterface::GraphicsApi api)
{
    switch(api) {
    case QSGRendererInterface::Software:return "software";
    case QSGRendererInterface::OpenGL:return "OpenGL";
    case QSGRendererInterface::Vulkan:return "Vulkan";
    case QSGRendererInterface::Metal:return "Metal";
    case QSGRendererInterface::Direct3D11:return "Direct3D11";
    case QSGRendererInterface::Direct3D12:return "Direct3D12";
    case QSGRendererInterface::Null:return "null";
    default:return "unknown";
    }
}
class Pane : public QQuickPaintedItem {
public:
    Pane(QQuickItem *parent,const Grid &initial,qreal cw,qreal ch):QQuickPaintedItem(parent),grid(initial),cw(cw),ch(ch) {
        setOpaquePainting(true);setWidth(80*cw+16);setHeight(24*ch+16);
    }
    void paint(QPainter *painter) override {
        const auto start=now();painter->fillRect(boundingRect(),QColor("#191d24"));
        painter->setRenderHint(QPainter::TextAntialiasing,true);
        paintTerminalGrid(*painter,grid,text,cw,ch,QPointF(8,8));
        painter->setPen(QColor("#a8c7fa"));painter->drawRect(QRectF(8+3*cw,8+4*ch,cw-1,ch-1));
        paintNs.fetch_add(now()-start,std::memory_order_relaxed);paints.fetch_add(1,std::memory_order_relaxed);
    }
    Grid grid;TerminalTextSurface text;qreal cw,ch;
    std::atomic<quint64> paints{0};std::atomic<qint64> paintNs{0};
};
struct Totals {quint64 rows=0,paints=0;qint64 paintNs=0,bytes=0;};
struct Probe {
    QGuiApplication &app;QQuickWindow window;QVector<Pane *> panes;Grid initial;QFont font;
    qreal cw=0,ch=0,ascent=0,scale=0;int sampleMs=300,index=0,warm=0,frames=0;bool pending=false,revealing=false,done=false;
    QSize viewport;QString profile,path;QStringList scenarios{"cached-scene","repaint","one-row","full","one-visible"};QJsonArray results;
    std::atomic<quint64> requested{0},rendered{0};quint64 generation=0;Totals before;
    qint64 stepStart=0,sampleStart=0,wallNs=0,updateNs=0,paintNs=0;quint64 rasterRows=0,paints=0;
    std::clock_t cpuStart{};
    Probe(QGuiApplication &app,int count,QString profile,int sampleMs,QString path):app(app),sampleMs(sampleMs),profile(profile),path(path) {
        initial=terminalBenchmarkFixture(profile=="unicode");
        const auto families=QFontDatabase::applicationFontFamilies(QFontDatabase::addApplicationFont(":/fonts/fira-code/FiraCode-Regular.ttf"));
        require(!families.isEmpty(),"bundled font");font=QFont(families.first());font.setPixelSize(18);
        QFontMetricsF metrics(font);cw=std::ceil(metrics.horizontalAdvance('M'));ch=std::ceil(metrics.height());ascent=metrics.ascent();
        window.setTitle("Workbench Scene Benchmark — synthetic terminals");window.setColor(QColor("#191d24"));window.resize(1120,680);
        for(int i=0;i<count;++i)panes.append(new Pane(window.contentItem(),initial,cw,ch));
        QObject::connect(&window,&QQuickWindow::sceneGraphError,&app,[this](auto,const QString &message){fail(message);});
        // Capture a generation while GUI state is synchronized; queued swap callbacks
        // from older frames cannot acknowledge a newly prepared generation.
        QObject::connect(&window,&QQuickWindow::beforeSynchronizing,&app,[this]{rendered.store(requested.load());},Qt::DirectConnection);
        QObject::connect(&window,&QQuickWindow::frameSwapped,&app,[this]{
            const auto token=rendered.load();const auto swapped=now();
            QMetaObject::invokeMethod(&this->app,[this,token,swapped]{guard([&]{finishFrame(token,swapped);});},Qt::QueuedConnection);
        },Qt::DirectConnection);
        QObject::connect(&app,&QGuiApplication::lastWindowClosed,&app,[this]{if(!done)fail("benchmark window closed before completion");});
        QTimer::singleShot(60000,&app,[this]{if(!done)fail("frame timeout: keep the native benchmark window visible and unobscured");});
        window.show();
        QTimer::singleShot(300,&app,[this]{guard([&]{
            require(window.isExposed(),"benchmark window is not exposed");scale=window.devicePixelRatio();viewport=window.size();
            const int cols=panes.size()==1?1:5,rows=panes.size()==1?1:3;
            const qreal pw=80*cw+16,ph=24*ch+16;
            const qreal zoom=std::min(qreal(1),std::min(window.width()/(cols*(pw+8)),window.height()/(rows*(ph+8))));
            for(int i=0;i<panes.size();++i) {auto *pane=panes[i];pane->setTransformOrigin(QQuickItem::TopLeft);pane->setScale(zoom);
                pane->setX((i%cols)*(pw+8)*zoom);pane->setY((i/cols)*(ph+8)*zoom);}
            beginScenario();
        });});
    }
    ~Probe() {QObject::disconnect(&window,nullptr,&app,nullptr);window.hide();}
    template<class F> void guard(F f) {if(done)return;try {f();}catch(const std::exception &e){fail(QString::fromUtf8(e.what()));}}
    void fail(const QString &message) {if(done)return;done=true;std::fprintf(stderr,"FAIL: %s\n",qPrintable(message));app.exit(1);}
    Totals totals() const {Totals t;for(auto *p:panes) {t.rows+=p->text.rasterizedRows();t.paints+=p->paints.load();t.paintNs+=p->paintNs.load();t.bytes+=p->text.imageBytes();}return t;}
    void beginScenario() {
        require(window.devicePixelRatio()==scale,"DPR changed during benchmark; rerun on a stationary display");
        for(int i=0;i<panes.size();++i) {auto *p=panes[i];p->setVisible(scenarios[index]!="one-visible" || i==0);
            p->grid=initial;p->text.update(p->grid,font,cw,ch,ascent,scale,p->isVisible());p->update();}
        warm=3;frames=0;wallNs=updateNs=paintNs=0;rasterRows=paints=0;step();
    }
    void step() {
        require(window.isExposed() && window.devicePixelRatio()==scale && window.size()==viewport,"window hidden, resized or DPR changed during sample");
        before=totals();stepStart=now();const auto scenario=scenarios[index];
        for(auto *p:panes) {
            if(scenario=="one-row")p->grid[12][0].text=generation%2?"B":"C";
            if(scenario=="full" || scenario=="one-visible")for(auto &row:p->grid)row[0].text=generation%2?"B":"C";
            if(scenario!="cached-scene") {p->text.update(p->grid,font,cw,ch,ascent,scale,p->isVisible());if(p->isVisible())p->update();}
        }
        if(!warm)updateNs+=now()-stepStart;
        pending=true;requested.store(++generation);window.update();
    }
    void finishFrame(quint64 token,qint64 swapped) {
        if(!pending || token!=generation)return;
        pending=false;const auto after=totals();
        if(revealing) {require(after.paints-before.paints==quint64(panes.size()),"reveal did not paint all panes");save(after.rows-before.rows,after.paints-before.paints);return;}
        const auto scenario=scenarios[index];const int visible=scenario=="one-visible"?1:panes.size();
        if(warm) {if(--warm==0) {sampleStart=now();cpuStart=std::clock();}step();return;}
        const quint64 expectedRows=scenario=="one-row"?visible:(scenario=="full" || scenario=="one-visible")?visible*24:0;
        const quint64 expectedPaints=scenario=="cached-scene"?0:visible;
        require(after.rows-before.rows==expectedRows,"unexpected dirty/hidden row count");
        require(after.paints-before.paints==expectedPaints,"unexpected visible/hidden paint count");
        rasterRows+=after.rows-before.rows;paints+=after.paints-before.paints;paintNs+=after.paintNs-before.paintNs;
        wallNs+=swapped-stepStart;++frames;
        if(frames<3 || now()-sampleStart<sampleMs*1000000LL) {step();return;}
        const double cpuNs=1e9*double(std::clock()-cpuStart)/CLOCKS_PER_SEC;
        results.append(QJsonObject{{"scenario",scenario},{"frames",frames},{"rasterized_rows",double(rasterRows)},{"paints",double(paints)},
            {"request_to_swap_ms",double(wallNs)/frames/1e6},{"gui_update_ms",double(updateNs)/frames/1e6},
            {"paint_ms",double(paintNs)/frames/1e6},{"cpu_ms_per_frame",cpuNs/frames/1e6},{"row_image_bytes",double(after.bytes)}});
        if(++index<scenarios.size()) {beginScenario();return;}
        // Reveal every hidden source with a distinct generation, outside timing.
        revealing=true;before=totals();
        for(int i=0;i<panes.size();++i) {auto *p=panes[i];if(i>0)for(auto &row:p->grid)row[0].text="Z";
            p->setVisible(true);p->text.update(p->grid,font,cw,ch,ascent,scale,true);p->update();}
        pending=true;requested.store(++generation);window.update();
    }
    void save(quint64 revealRows,quint64 revealPaints) {
        require(revealRows==quint64((panes.size()-1)*24),"hidden reveal did not catch up exactly");
        const auto api=apiName(window.rendererInterface()->graphicsApi());require(api!="null" && api!="unknown","no usable scene graph backend");
        const auto image=window.grabWindow();require(!image.isNull() && image.save(path+".png"),"save rendered scene capture");
        QJsonObject report{{"scope","Qt Quick painted-item scene with synthetic Ghostty grids; no PTYs, sockets, GPU timestamps or measured VRAM"},
            {"qt",qVersion()},{"compiler",__VERSION__},{"platform",QGuiApplication::platformName()},{"graphics_api",api},{"build_type","Release"},
            {"sample_ms",sampleMs},{"warmup_frames",3},{"panes",panes.size()},{"columns",80},{"rows",24},{"profile",profile},{"device_pixel_ratio",scale},
            {"window_width",window.width()},{"window_height",window.height()},{"cell_width",cw},{"cell_height",ch},
            {"item_scale",panes.first()->scale()},{"item_width",panes.first()->width()},{"item_height",panes.first()->height()},
            {"estimated_rgba_item_bytes",double(panes.size())*qCeil((80*cw+16)*scale)*qCeil((24*ch+16)*scale)*4},
            {"capture_width",image.width()},{"capture_height",image.height()},
            {"reveal_rasterized_rows",double(revealRows)},{"reveal_paints",double(revealPaints)},{"scenarios",results}};
        QSaveFile file(path);require(file.open(QIODevice::WriteOnly),"open scene report");const auto bytes=QJsonDocument(report).toJson();
        require(file.write(bytes)==bytes.size() && file.commit(),"save scene report");
        done=true;std::printf("PASS: %d panes, %s, %.2fx, %s; cached/dirty/hidden paint and reveal counts\n",int(panes.size()),qPrintable(api),scale,qPrintable(profile));app.exit(0);
    }
};
}
int main(int argc,char **argv)
{
    QGuiApplication app(argc,argv);app.setApplicationName("Workbench Scene Benchmark");
    try {
#ifndef NDEBUG
        throw std::runtime_error("scene benchmark requires a Release build");
#endif
        const auto args=app.arguments();require(args.size()==5,"usage: workbench-scene-bench PANES PROFILE SAMPLE_MS REPORT");
        bool ok=false;const int count=args[1].toInt(&ok);require(ok && (count==1 || count==15),"pane count");
        require(args[2]=="ascii" || args[2]=="unicode","profile");const int ms=args[3].toInt(&ok);require(ok && ms>=100 && ms<=5000,"sample interval");
        Probe probe(app,count,args[2],ms,args[4]);return app.exec();
    }catch(const std::exception &e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
