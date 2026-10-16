// CPU row-raster/cache/compositing benchmark, not a Qt Quick/GPU/PTY benchmark.
#include "terminal_text.h"
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
Grid fixture(bool unicode)
{
    GhosttyTerminal terminal=nullptr;GhosttyRenderState render=nullptr;
    GhosttyRenderStateRowIterator iterator=nullptr;GhosttyRenderStateRowCells cells=nullptr;
    struct Cleanup {
        GhosttyTerminal &terminal;GhosttyRenderState &render;GhosttyRenderStateRowIterator &iterator;GhosttyRenderStateRowCells &cells;
        ~Cleanup(){ghostty_render_state_row_cells_free(cells);ghostty_render_state_row_iterator_free(iterator);ghostty_render_state_free(render);ghostty_terminal_free(terminal);}
    } cleanup{terminal,render,iterator,cells};
    require(ghostty_terminal_new(nullptr,&terminal,columns,rows)==GHOSTTY_SUCCESS,"create VT fixture");
    for(int row=0;row<rows;++row) {
        const auto text=QString("\x1b[%1;1HA %2 ").arg(row+1).arg(row,2,10,QChar('0'))+
            (unicode?QString::fromUtf8("compile -> === café 日本語 👩🏽‍💻 नमस्ते"):
                QString("compile -> != === src/terminal.rs 0123456789 passed ................."));
        const auto bytes=text.toUtf8();ghostty_terminal_vt_write(terminal,reinterpret_cast<const uint8_t *>(bytes.constData()),bytes.size());
    }
    require(ghostty_render_state_new(nullptr,&render)==GHOSTTY_SUCCESS,"create fixture render state");
    require(ghostty_render_state_row_iterator_new(nullptr,&iterator)==GHOSTTY_SUCCESS,"create fixture row iterator");
    require(ghostty_render_state_row_cells_new(nullptr,&cells)==GHOSTTY_SUCCESS,"create fixture cell iterator");
    require(ghostty_render_state_update(render,terminal)==GHOSTTY_SUCCESS,"update fixture");
    ghostty_render_state_get(render,GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR,&iterator);
    Grid grid;
    while(ghostty_render_state_row_iterator_next(iterator)) {
        QVector<TerminalTextCell> row;ghostty_render_state_row_get(iterator,GHOSTTY_RENDER_STATE_ROW_DATA_CELLS,&cells);
        while(ghostty_render_state_row_cells_next(cells)) {
            TerminalTextCell cell;cell.fg=QColor("#e4e8ef");cell.bg=QColor("#191d24");
            GhosttyCell raw=0;ghostty_render_state_row_cells_get(cells,GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_RAW,&raw);
            ghostty_cell_get(raw,GHOSTTY_CELL_DATA_WIDE,&cell.wide);
            GhosttyBuffer buffer{};
            auto status=ghostty_render_state_row_cells_get(cells,GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_UTF8,&buffer);
            if(status==GHOSTTY_OUT_OF_SPACE && buffer.len<=65536) {
                QByteArray text(qsizetype(buffer.len),0);buffer.ptr=reinterpret_cast<uint8_t *>(text.data());buffer.cap=text.size();
                require(ghostty_render_state_row_cells_get(cells,GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_UTF8,&buffer)==GHOSTTY_SUCCESS,"fixture UTF-8");
                cell.text=QString::fromUtf8(text.constData(),qsizetype(buffer.len));
            } else require(status==GHOSTTY_SUCCESS,"fixture cell sizing");
            row.append(cell);
        }
        require(row.size()==columns,"fixture width");grid.append(row);
    }
    require(grid.size()==rows,"fixture height");return grid;
}
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
        const auto initial=fixture(args[3]=="unicode");QJsonArray results;
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
