// CPU row-raster/cache/compositing benchmark, not a Qt Quick/GPU/PTY benchmark.
#include "terminal_text.h"
#include "terminal_bench_fixture.h"
#include <QGuiApplication>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QPainter>
#include <cmath>
#include <ctime>
#include <cstdio>
#include <stdexcept>

namespace {
constexpr int columns=80,rows=24;
void require(bool ok,const char *message) {if(!ok)throw std::runtime_error(message);}
using Grid=QVector<QVector<TerminalTextCell>>;
struct Pane {Grid grid;TerminalTextSurface text;QImage target;};
void paint(Pane &pane,qreal ch)
{
    pane.target.fill(QColor("#191d24"));QPainter painter(&pane.target);
    // Only cached text image compositing; live per-cell backgrounds/cursor/chrome
    // and the Qt Quick texture upload are deliberately outside this measurement.
    for(int row=0;row<pane.text.size();++row)painter.drawImage(QPointF(0,row*ch),pane.text.image(row));
}
}
int main(int argc,char **argv)
{
    QGuiApplication app(argc,argv);app.setApplicationName("Workbench Raster Benchmark");
    try {
#ifndef NDEBUG
        throw std::runtime_error("benchmark requires a Release build");
#endif
        const auto args=app.arguments();require(args.size()==6,"usage: workbench-text-bench PANES SCALE PROFILE SAMPLE_MS REPORT");
        bool ok=false;const int count=args[1].toInt(&ok);require(ok && (count==1 || count==15),"pane count");
        const qreal scale=args[2].toDouble(&ok);require(ok && (scale==1 || scale==2),"scale");
        require(args[3]=="ascii" || args[3]=="unicode","profile");
        const int sampleMs=args[4].toInt(&ok);require(ok && sampleMs>=100 && sampleMs<=5000,"sample interval");
        const auto families=QFontDatabase::applicationFontFamilies(QFontDatabase::addApplicationFont(":/fonts/fira-code/FiraCode-Regular.ttf"));
        require(!families.isEmpty(),"bundled font");QFont font(families.first());font.setPixelSize(18);
        const QFontMetricsF metrics(font);const qreal cw=std::ceil(metrics.horizontalAdvance('M')),ch=std::ceil(metrics.height());
        const auto initial=terminalBenchmarkFixture(args[3]=="unicode");QJsonArray results;
        for(const QString scenario:{"retained","one-row","full","one-visible","hidden"}) {
            QVector<Pane> panes(count);qint64 cacheBytes=0,targetBytes=0;
            for(auto &pane:panes) {
                pane.grid=initial;pane.text.update(pane.grid,font,cw,ch,metrics.ascent(),scale,true);
                pane.target=QImage(qCeil(columns*cw*scale),qCeil(rows*ch*scale),QImage::Format_ARGB32_Premultiplied);
                require(!pane.target.isNull(),"composite target allocation");pane.target.setDevicePixelRatio(scale);paint(pane,ch);
                cacheBytes+=pane.text.imageBytes();targetBytes+=pane.target.sizeInBytes();
            }
            quint64 initialRaster=0;for(const auto &pane:panes)initialRaster+=pane.text.rasterizedRows();
            QElapsedTimer timer;timer.start();const auto cpuStart=std::clock();
            int frames=0;quint64 composites=0;
            do {
                for(int index=0;index<count;++index) {
                    auto &pane=panes[index];
                    if(scenario!="retained") {
                        const QString text=frames%2==0?"B":"C";
                        if(scenario=="one-row")pane.grid[rows/2][0].text=text;
                        else for(auto &row:pane.grid)row[0].text=text;
                    }
                    const bool visible=scenario!="hidden" && (scenario!="one-visible" || index==0);
                    pane.text.update(pane.grid,font,cw,ch,metrics.ascent(),scale,visible);
                    if(visible) {paint(pane,ch);++composites;}
                }
                ++frames;
            } while((frames<3 || timer.elapsed()<sampleMs) && frames<20000);
            const auto wallNs=timer.nsecsElapsed();const auto cpuEnd=std::clock();
            const double cpuNs=1e9*double(cpuEnd-cpuStart)/CLOCKS_PER_SEC;
            quint64 raster=0;for(const auto &pane:panes)raster+=pane.text.rasterizedRows();raster-=initialRaster;
            const quint64 expected=scenario=="retained" || scenario=="hidden"?0:
                quint64(frames)*(scenario=="one-row"?count:scenario=="one-visible"?rows:count*rows);
            require(raster==expected,"unexpected rasterized row count");
            require(composites==quint64(frames)*(scenario=="hidden"?0:scenario=="one-visible"?1:count),"unexpected composites");
            double revealNs=0;quint64 revealRows=0;
            if(scenario=="hidden" || scenario=="one-visible") {
                // Force content distinct from every warm/timed generation so the
                // reveal test is independent of the loop's final toggle parity.
                for(auto &pane:panes)for(auto &row:pane.grid)row[0].text="Z";
                quint64 before=0;for(const auto &pane:panes)before+=pane.text.rasterizedRows();
                QElapsedTimer reveal;reveal.start();
                for(auto &pane:panes) {pane.text.update(pane.grid,font,cw,ch,metrics.ascent(),scale,true);paint(pane,ch);}
                revealNs=reveal.nsecsElapsed();
                for(const auto &pane:panes)revealRows+=pane.text.rasterizedRows();
                revealRows-=before;
                require(revealRows==quint64(count*rows),"reveal did not catch up every row");
            }
            results.append(QJsonObject{{"scenario",scenario},{"frames",frames},{"wall_ns",double(wallNs)},
                {"cpu_ns",cpuNs},{"wall_ms_per_frame",double(wallNs)/frames/1e6},{"cpu_ms_per_frame",cpuNs/frames/1e6},
                {"rasterized_rows",double(raster)},{"composites",double(composites)},
                {"row_image_bytes",double(cacheBytes)},{"composite_image_bytes",double(targetBytes)},
                {"reveal_ns",revealNs},{"reveal_rasterized_rows",double(revealRows)}});
        }
        QJsonObject report{{"scope","CPU row raster/cache/image compositing only; no GPU, Qt Quick, PTY or live background/cursor paint"},
            {"qt",qVersion()},{"platform",QGuiApplication::platformName()},{"build_type","Release"},{"compiler",__VERSION__},
            {"panes",count},{"columns",columns},{"rows",rows},{"scale",scale},{"profile",args[3]},
            {"cell_width",cw},{"cell_height",ch},{"sample_ms",sampleMs},{"scenarios",results}};
        QSaveFile file(args[5]);require(file.open(QIODevice::WriteOnly),"open benchmark report");const auto data=QJsonDocument(report).toJson();
        require(file.write(data)==data.size() && file.commit(),"save benchmark report");
        std::printf("PASS: %d panes, %.0fx, %s; retained/hidden counts, exact dirty rows and reveal catch-up\n",count,scale,qPrintable(args[3]));return 0;
    } catch(const std::exception &error) {std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;}
}
