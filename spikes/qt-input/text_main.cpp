// Offline shaping evidence only: no runtime, PTY, catalog or renderer adoption.
#include <QGuiApplication>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QTextLayout>
#include <QGlyphRun>
#include <QRawFont>
#include <QPainter>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QDir>
#include <QDebug>
#include <ghostty/vt.h>
#include "terminal_text.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include <stdexcept>
#include <cstdio>

namespace {
void require(bool ok,const char *message) { if(!ok)throw std::runtime_error(message); }
struct Cell {int column;QString text;GhosttyCellWide wide;};
struct Sample {const char *id,*label;QString text;};
QVector<Cell> cellsFor(const QString &text)
{
    GhosttyTerminal terminal=nullptr;GhosttyRenderState render=nullptr;
    GhosttyRenderStateRowIterator rows=nullptr;GhosttyRenderStateRowCells cells=nullptr;
    // RAII also covers errors after partially initializing the Ghostty handles.
    struct Cleanup {
        GhosttyTerminal &terminal;GhosttyRenderState &render;
        GhosttyRenderStateRowIterator &rows;GhosttyRenderStateRowCells &cells;
        ~Cleanup() {ghostty_render_state_row_cells_free(cells);ghostty_render_state_row_iterator_free(rows);
            ghostty_render_state_free(render);ghostty_terminal_free(terminal);}
    } cleanup{terminal,render,rows,cells};
    require(ghostty_terminal_new(nullptr,&terminal,80,2)==GHOSTTY_SUCCESS,"VT creation");
    const auto bytes=text.toUtf8();ghostty_terminal_vt_write(terminal,reinterpret_cast<const uint8_t *>(bytes.constData()),bytes.size());
    require(ghostty_render_state_new(nullptr,&render)==GHOSTTY_SUCCESS,"render state creation");
    require(ghostty_render_state_row_iterator_new(nullptr,&rows)==GHOSTTY_SUCCESS,"row iterator creation");
    require(ghostty_render_state_row_cells_new(nullptr,&cells)==GHOSTTY_SUCCESS,"cell iterator creation");
    require(ghostty_render_state_update(render,terminal)==GHOSTTY_SUCCESS,"render state update");
    require(ghostty_render_state_get(render,GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR,&rows)==GHOSTTY_SUCCESS,"rows");
    require(ghostty_render_state_row_iterator_next(rows),"first row");
    require(ghostty_render_state_row_get(rows,GHOSTTY_RENDER_STATE_ROW_DATA_CELLS,&cells)==GHOSTTY_SUCCESS,"cells");
    QVector<Cell> result;int column=0;
    while(ghostty_render_state_row_cells_next(cells)) {
        GhosttyBuffer buffer{};
        auto status=ghostty_render_state_row_cells_get(cells,GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_UTF8,&buffer);
        if(status==GHOSTTY_OUT_OF_SPACE && buffer.len>0 && buffer.len<=65536) {
            QByteArray output(qsizetype(buffer.len),0);buffer.ptr=reinterpret_cast<uint8_t *>(output.data());buffer.cap=output.size();
            require(ghostty_render_state_row_cells_get(cells,GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_UTF8,&buffer)==GHOSTTY_SUCCESS,"UTF-8 cell extraction");
            auto value=QString::fromUtf8(output.constData(),qsizetype(buffer.len));
            GhosttyCell raw=0;GhosttyCellWide wide=GHOSTTY_CELL_WIDE_NARROW;
            ghostty_render_state_row_cells_get(cells,GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_RAW,&raw);
            ghostty_cell_get(raw,GHOSTTY_CELL_DATA_WIDE,&wide);
            if(!value.isEmpty())result.push_back({column,value,wide});
        } else require(status==GHOSTTY_SUCCESS,"UTF-8 cell sizing");
        ++column;
    }
    QString restored;for(const auto &cell:result)restored+=cell.text;
    require(restored==text,"VT cell extraction changed specimen text");
    return result;
}
std::unique_ptr<QTextLayout> layoutFor(const QString &text,const QFont &font)
{
    auto layout=std::make_unique<QTextLayout>(text,font);
    QTextOption option;option.setWrapMode(QTextOption::NoWrap);
    // The reference uses Unicode's natural direction, unlike fixed terminal cells.
    option.setTextDirection(Qt::LayoutDirectionAuto);layout->setTextOption(option);
    layout->beginLayout();auto line=layout->createLine();
    require(line.isValid(),"text layout line");line.setLineWidth(600);layout->endLayout();
    return layout;
}
QJsonArray glyphs(const QTextLayout &layout,int offset=0)
{
    QJsonArray result;
    for(const auto &run:layout.glyphRuns(-1,-1,QTextLayout::RetrieveAll)) {
        const auto indexes=run.glyphIndexes();const auto positions=run.positions();const auto strings=run.stringIndexes();
        require(indexes.size()==positions.size() && indexes.size()==strings.size(),"glyph metadata lengths");
        for(qsizetype i=0;i<indexes.size();++i)
            result.append(QJsonObject{{"font",run.rawFont().familyName()},{"style",run.rawFont().styleName()},
                {"glyph",int(indexes[i])},{"utf16",int(strings[i])+offset},
                {"x",positions[i].x()},{"y",positions[i].y()}});
    }
    return result;
}
QStringList identities(const QJsonArray &glyphs)
{
    QStringList result;
    for(const auto &entry:glyphs) {const auto value=entry.toObject();
        result.append(value["font"].toString()+":"+value["style"].toString()+":"+QString::number(value["glyph"].toInt()));}
    result.sort();return result;
}
QStringList fonts(const QJsonArray &glyphs)
{
    QStringList result;for(const auto &entry:glyphs)result.append(entry.toObject()["font"].toString());
    result.removeDuplicates();result.sort();return result;
}
int missing(const QJsonArray &glyphs)
{
    int count=0;for(const auto &entry:glyphs)if(entry.toObject()["glyph"].toInt()==0)++count;return count;
}
void writeJson(const QString &path,const QJsonObject &data)
{
    QSaveFile file(path);require(file.open(QIODevice::WriteOnly),"open JSON artifact");
    const auto bytes=QJsonDocument(data).toJson();require(file.write(bytes)==bytes.size() && file.commit(),"save JSON artifact");
}
}
int main(int argc,char **argv)
{
    QGuiApplication app(argc,argv);app.setApplicationName("Workbench Text Proof");
    try {
        const auto args=app.arguments();require(args.size()==3 && args[1]=="--output","usage: workbench-text-probe --output DIRECTORY");
        const QDir output(args[2]);require(QDir().mkpath(output.absolutePath()),"create artifact directory");
        const int fontId=QFontDatabase::addApplicationFont(":/fonts/fira-code/FiraCode-Regular.ttf");
        const auto families=QFontDatabase::applicationFontFamilies(fontId);require(!families.isEmpty(),"load bundled Fira Code");
        QFont font(families.first());font.setPixelSize(18);
        require(terminalTextSelfTest(font),"grid renderer invariants");
        const QFontMetricsF metrics(font);const qreal cellWidth=std::ceil(metrics.horizontalAdvance('M'));
        const qreal cellHeight=std::ceil(metrics.height());
        const QVector<Sample> samples={
            {"ascii","ASCII / programming ligatures","office -> != === 0123456789"},
            {"combining","Combining accents",QString::fromUtf8("café Å ñ")},
            {"arabic","Arabic / joining",QString::fromUtf8("مرحبا بالعالم")},
            {"indic","Devanagari / conjuncts",QString::fromUtf8("नमस्ते दुनिया")},
            {"cjk","CJK / wide cells",QString::fromUtf8("日本語 中文 한국어")},
            {"emoji","Emoji / ZWJ / skin tone",QString::fromUtf8("👩🏽‍💻 👨‍👩‍👧‍👦 ❤️ 🇳🇱")},
            {"box","Box drawing",QString::fromUtf8("┌──────┬──────┐ │ ├─┼─┤ └──────┘")},
            {"long-cluster","Long grapheme (25 combining marks)",QString("e")+QString(25,QChar(0x301))}
        };
        struct Prepared {Sample sample;QVector<Cell> cells;std::unique_ptr<QTextLayout> layout;QJsonObject report;};
        std::vector<Prepared> prepared;QJsonArray reports;bool joiningDifference=false;
        for(const auto &sample:samples) {
            auto cells=cellsFor(sample.text);auto layout=layoutFor(sample.text,font);
            auto reference=glyphs(*layout);QJsonArray isolated,cellReport;int utf16=0;
            for(const auto &cell:cells) {
                auto piece=layoutFor(cell.text,font);
                for(const auto &glyph:glyphs(*piece,utf16))isolated.append(glyph);
                cellReport.append(QJsonObject{{"column",cell.column},{"text",cell.text},{"utf16",utf16}});
                utf16+=cell.text.size();
            }
            const bool different=identities(reference)!=identities(isolated);
            if(QString(sample.id)=="arabic" && missing(reference)==0)joiningDifference=different;
            if(QString(sample.id)=="ascii")require(missing(reference)==0,"bundled font lacks basic ASCII");
            QJsonObject report{{"id",sample.id},{"label",sample.label},{"text",sample.text},
                {"cells",cellReport},{"isolated_glyphs",isolated},{"reference_glyphs",reference},
                {"isolated_missing_glyphs",missing(isolated)},{"reference_missing_glyphs",missing(reference)},
                {"glyph_identity_difference",different},{"reference_fonts",QJsonArray::fromStringList(fonts(reference))}};
            reports.append(report);prepared.push_back({sample,std::move(cells),std::move(layout),report});
        }
        // A missing Arabic fallback is reported as a coverage gap, not mislabeled
        // as successful joining. Other font inventories may yield different glyph IDs.
        QJsonArray captures,gridReports;
        constexpr int width=1400,height=940;const int rowHeight=100;
        for(qreal scale:{1.,1.25,1.5,2.}) for(bool integrated:{false,true}) {
            QImage image(qRound(width*scale),qRound(height*scale),QImage::Format_ARGB32_Premultiplied);
            require(!image.isNull(),"image allocation");image.setDevicePixelRatio(scale);image.fill(QColor("#191d24"));
            QPainter painter(&image);painter.setRenderHint(QPainter::TextAntialiasing);
            QFont label=font;label.setPixelSize(14);painter.setFont(label);painter.setPen(QColor("#c9d2e3"));
            painter.drawText(20,25,"Workbench text proof · Fira Code 18 px + system fallback · scale "+QString::number(scale));
            painter.drawText(20,50,integrated?"Integrated grid shaping (RTL retains per-cell order)":"Previous Qt terminal: one draw per Ghostty cell");
            painter.drawText(720,50,"Qt full-text reference: natural direction/advance, not a terminal layout");
            for(size_t i=0;i<prepared.size();++i) {
                const auto &item=prepared[i];const qreal y=80+i*rowHeight;
                painter.setFont(label);painter.setPen(QColor("#a8c7fa"));
                painter.drawText(20,y,QString(item.sample.label));
                painter.drawText(720,y,"Fonts: "+fonts(item.report["reference_glyphs"].toArray()).join(", "));
                const qreal top=y+10,baseline=top+metrics.ascent();
                painter.setPen(QColor("#303844"));
                for(int col=0;col<=52;++col)painter.drawLine(QPointF(20+col*cellWidth,top),QPointF(20+col*cellWidth,top+cellHeight));
                painter.drawLine(QPointF(20,top+cellHeight),QPointF(20+52*cellWidth,top+cellHeight));
                painter.save();painter.setClipRect(QRectF(20,top-5,650,cellHeight+12));
                painter.setFont(font);painter.setPen(QColor("#e4e8ef"));
                if(integrated) {
                    QVector<TerminalTextCell> row(80);
                    for(auto &cell:row) {cell.fg=QColor("#e4e8ef");cell.bg=QColor("#191d24");}
                    for(const auto &cell:item.cells) {row[cell.column].text=cell.text;row[cell.column].wide=cell.wide;
                        if(cell.wide==GHOSTTY_CELL_WIDE_WIDE && cell.column+1<row.size())row[cell.column+1].wide=GHOSTTY_CELL_WIDE_SPACER_TAIL;}
                    QVector<TerminalTextPlacement> placements;
                    auto rendered=renderTerminalTextRow(row,font,cellWidth,cellHeight,metrics.ascent(),scale,scale==1?&placements:nullptr);
                    if(scale==1) {
                        QJsonArray placed;QStringList actual,expected;
                        for(const auto &glyph:placements) {
                            placed.append(QJsonObject{{"first_column",glyph.firstColumn},{"last_column",glyph.lastColumn},
                                {"font",glyph.family},{"glyph",int(glyph.glyph)},{"x",glyph.position.x()},{"y",glyph.position.y()},{"shaped",glyph.shaped}});
                            actual.append(glyph.family+":"+QString::number(glyph.glyph));
                        }
                        if(QString(item.sample.id)!="arabic") {
                            for(const auto &entry:item.report["reference_glyphs"].toArray()) {const auto glyph=entry.toObject();expected.append(glyph["font"].toString()+":"+QString::number(glyph["glyph"].toInt()));}
                            actual.sort();expected.sort();require(actual==expected,"grid glyphs differ from full shaping in an LTR specimen");
                        }
                        gridReports.append(QJsonObject{{"id",item.sample.id},{"placements",placed}});
                    }
                    painter.drawImage(QPointF(20,top),rendered);
                } else for(const auto &cell:item.cells)painter.drawText(QPointF(20+cell.column*cellWidth,baseline),cell.text);
                painter.restore();painter.save();painter.setClipRect(QRectF(720,top-5,650,cellHeight+12));
                painter.setPen(QColor("#e4e8ef"));
                item.layout->draw(&painter,QPointF(720,baseline-item.layout->lineAt(0).ascent()));painter.restore();
                painter.setFont(label);painter.setPen(QColor("#abb5c7"));
                painter.drawText(20,y+76,"Baseline cell vs reference glyph difference: "+QString(item.report["glyph_identity_difference"].toBool()?"yes":"no")+
                    " · missing glyphs (cell / full): "+QString::number(item.report["isolated_missing_glyphs"].toInt())+" / "+QString::number(item.report["reference_missing_glyphs"].toInt()));
            }
            painter.setFont(label);painter.setPen(QColor("#abb5c7"));
            painter.drawText(20,915,"Visual evidence only. Font coverage and glyph differences do not prove correct shaping, terminal placement or physical-display sharpness.");
            painter.end();const auto name=QString(integrated?"grid-":"comparison-")+QString::number(scale)+"x.png";
            require(image.save(output.filePath(name)),"save comparison PNG");
            captures.append(QJsonObject{{"file",name},{"integrated",integrated},{"scale",scale},{"width",image.width()},{"height",image.height()}});
        }
        writeJson(output.filePath("report.json"),{{"qt",qVersion()},{"platform",QGuiApplication::platformName()},
            {"glyph_utf16_indexes","raw Qt diagnostic values; fallback indexes are not used for terminal cell mapping"},{"base_font",families.first()},{"font_pixel_size",font.pixelSize()},{"cell_width",cellWidth},{"cell_height",cellHeight},
            {"arabic_joining_difference_observed",joiningDifference},{"samples",reports},{"grid",gridReports},{"captures",captures}});
        std::printf("PASS: VT text preserved, full cell graphemes and Qt glyph/font evidence exported\n");
        std::printf("Arabic joining difference observed: %s (inspect report for coverage)\n",joiningDifference?"yes":"no");
        std::printf("Artifacts: %s\n",qPrintable(output.absolutePath()));return 0;
    } catch(const std::exception &error) {qCritical()<<"FAIL: text proof:"<<error.what();return 1;}
}
