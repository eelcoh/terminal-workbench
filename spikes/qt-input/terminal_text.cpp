#include "terminal_text.h"
#include <QTextLayout>
#include <QTextBoundaryFinder>
#include <QGlyphRun>
#include <QRawFont>
#include <QPainter>
#include <QHash>
#include <algorithm>
#include <cmath>

namespace {
bool sameStyle(const TerminalTextCell &a,const TerminalTextCell &b)
{
    return a.fg==b.fg && a.bg==b.bg && a.bold==b.bold && a.italic==b.italic &&
        a.underline==b.underline && a.selected==b.selected;
}
bool spacer(const TerminalTextCell &c) {return c.wide==GHOSTTY_CELL_WIDE_SPACER_HEAD || c.wide==GHOSTTY_CELL_WIDE_SPACER_TAIL;}
bool needsDirectionPolicy(const QString &text)
{
    for(qsizetype index=0;index<text.size();++index) {
        char32_t scalar=text[index].unicode();
        if(text[index].isHighSurrogate() && index+1<text.size() && text[index+1].isLowSurrogate()) {
            scalar=QChar::surrogateToUcs4(text[index],text[index+1]);++index;
        }
        const auto d=QChar::direction(scalar);
        if(d==QChar::DirR || d==QChar::DirAL || d==QChar::DirRLE || d==QChar::DirRLO ||
            d==QChar::DirLRE || d==QChar::DirLRO || d==QChar::DirPDF ||
            d==QChar::DirRLI || d==QChar::DirLRI || d==QChar::DirFSI || d==QChar::DirPDI)return true;
    }
    return false;
}
struct Key {
    int font,occurrence;quint32 glyph;QPointF position;
    bool operator==(const Key &b) const {return font==b.font && occurrence==b.occurrence && glyph==b.glyph && position==b.position;}
    friend size_t qHash(const Key &key,size_t seed=0) {return qHashMulti(seed,key.font,key.occurrence,key.glyph,key.position.x(),key.position.y());}
};
struct Glyph {Key key;int owner;};
void shape(QPainter &painter,const QVector<TerminalTextCell> &cells,int begin,int end,
    QFont font,qreal cw,qreal ch,qreal ascent,QVector<TerminalTextPlacement> *diagnostics)
{
    font.setBold(cells[begin].bold);font.setItalic(cells[begin].italic);font.setUnderline(false);
    painter.setFont(font);painter.setPen(cells[begin].fg);
    QString text;QVector<int> columns,offsets;
    for(int column=begin;column<end;++column)if(!spacer(cells[column]) && !cells[column].text.isEmpty()) {
        columns.append(column);offsets.append(text.size());text+=cells[column].text;
    }
    offsets.append(text.size());
    const bool directionFallback=needsDirectionPolicy(text);
    if(directionFallback) {
        // Preserve the previous per-cell terminal order until RTL is designed.
        for(const int column:columns) {
            painter.drawText(QPointF(column*cw,ascent),cells[column].text);
            if(diagnostics)diagnostics->append({column,column+(cells[column].wide==GHOSTTY_CELL_WIDE_WIDE?2:1),0,QPointF(column*cw,0),font.family(),false});
        }
    } else {
        QTextLayout layout(text,font);QTextOption option;option.setWrapMode(QTextOption::NoWrap);
        option.setTextDirection(Qt::LeftToRight);layout.setTextOption(option);
        layout.beginLayout();auto line=layout.createLine();if(line.isValid())line.setLineWidth(1000000);layout.endLayout();
        if(!line.isValid())return;
        QVector<int> parent;for(int i=0;i<columns.size();++i)parent.append(i);
        auto root=[&](int i) {while(parent[i]!=i) {parent[i]=parent[parent[i]];i=parent[i];}return i;};
        auto join=[&](int a,int b) {a=root(a);b=root(b);if(a!=b)parent[std::max(a,b)]=std::min(a,b);};
        // Unicode graphemes may span several VT cells (notably emoji sequences).
        QTextBoundaryFinder boundary(QTextBoundaryFinder::Grapheme,text);int first=0,last=0;
        boundary.toStart();
        for(int finish=boundary.toNextBoundary();finish>=0;finish=boundary.toNextBoundary()) {
            while(first+1<columns.size() && offsets[first+1]<last)++first;
            for(int i=first+1;i<columns.size() && offsets[i]<finish;++i)join(first,i);
            last=finish;while(first+1<columns.size() && offsets[first+1]<=finish)++first;
        }
        QVector<QRawFont> fonts;QVector<Glyph> glyphs;QHash<Key,int> seen;
        for(int cell=0;cell<columns.size();++cell) {
            QHash<Key,int> occurrences;
            // Public source-range queries avoid unreliable fallback stringIndexes.
            const auto runs=layout.glyphRuns(offsets[cell],offsets[cell+1]-offsets[cell],
                QTextLayout::RetrieveGlyphIndexes|QTextLayout::RetrieveGlyphPositions);
            for(const auto &run:runs) {
                auto raw=run.rawFont();int fontIndex=fonts.indexOf(raw);
                if(fontIndex<0) {fontIndex=fonts.size();fonts.append(raw);}
                const auto indexes=run.glyphIndexes();const auto positions=run.positions();
                for(int i=0;i<indexes.size();++i) {
                    Key key{fontIndex,0,indexes[i],positions[i]};
                    const int occurrence=occurrences.value(key,0);occurrences.insert(key,occurrence+1);key.occurrence=occurrence;
                    const auto found=seen.constFind(key);
                    if(found!=seen.cend()) {
                        const int firstOwner=glyphs[*found].owner;
                        for(int member=firstOwner+1;member<=cell;++member)join(firstOwner,member);
                    }
                    else {seen.insert(key,glyphs.size());glyphs.append({key,cell});}
                }
            }
        }
        QVector<int> lastCell(columns.size());for(int i=0;i<columns.size();++i)lastCell[root(i)]=i;
        for(const auto &glyph:glyphs) {
            const int firstCell=root(glyph.owner),last=lastCell[firstCell];
            const qreal nativeAnchor=line.cursorToX(offsets[firstCell]);
            const auto position=glyph.key.position+QPointF(columns[firstCell]*cw-nativeAnchor,ascent-line.ascent());
            QGlyphRun run;run.setRawFont(fonts[glyph.key.font]);run.setGlyphIndexes({glyph.key.glyph});run.setPositions({position});
            painter.drawGlyphRun(QPointF(),run);
            if(diagnostics)diagnostics->append({columns[firstCell],columns[last]+(cells[columns[last]].wide==GHOSTTY_CELL_WIDE_WIDE?2:1),glyph.key.glyph,position,fonts[glyph.key.font].familyName(),true});
        }
    }
    if(cells[begin].underline) {
        const qreal y=std::min(ch-1,ascent+2);
        painter.drawLine(QPointF(begin*cw,y),QPointF(end*cw,y));
    }
}
}
QImage renderTerminalTextRow(const QVector<TerminalTextCell> &cells,const QFont &font,
    qreal cw,qreal ch,qreal ascent,qreal scale,QVector<TerminalTextPlacement> *diagnostics)
{
    QImage image(std::max(1,qCeil(cells.size()*cw*scale)),std::max(1,qCeil(ch*scale)),QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(scale);image.fill(Qt::transparent);
    if(diagnostics)diagnostics->clear();
    QPainter painter(&image);painter.setRenderHint(QPainter::TextAntialiasing);
    for(int begin=0;begin<cells.size();) {
        if(cells[begin].text.isEmpty() || spacer(cells[begin])) {++begin;continue;}
        int end=begin+1;
        while(end<cells.size()) {
            if(spacer(cells[end])) {++end;continue;}
            if(!sameStyle(cells[begin],cells[end]) || cells[end].text.isEmpty())break;
            ++end;
        }
        painter.save();painter.setClipRect(QRectF(begin*cw,0,(end-begin)*cw,ch));
        shape(painter,cells,begin,end,font,cw,ch,ascent,diagnostics);painter.restore();begin=end;
    }
    return image;
}
bool TerminalTextRow::update(const QVector<TerminalTextCell> &cells,const QFont &font,qreal cw,qreal ch,qreal ascent,qreal scale)
{
    if(revisions_ && cells_==cells && font_==font && width_==cw && height_==ch && ascent_==ascent && scale_==scale)return false;
    image_=renderTerminalTextRow(cells,font,cw,ch,ascent,scale);cells_=cells;font_=font;
    width_=cw;height_=ch;ascent_=ascent;scale_=scale;++revisions_;return true;
}

void TerminalTextSurface::update(const QVector<QVector<TerminalTextCell>> &grid,const QFont &font,
    qreal cw,qreal ch,qreal ascent,qreal scale,bool presentable)
{
    if(!presentable)return;
    rows_.resize(grid.size());
    for(int row=0;row<grid.size();++row)if(rows_[row].update(grid[row],font,cw,ch,ascent,scale))++rasterizedRows_;
}
qint64 TerminalTextSurface::imageBytes() const
{
    qint64 result=0;for(const auto &row:rows_)result+=row.image().sizeInBytes();return result;
}
