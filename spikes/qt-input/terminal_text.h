#pragma once
#include <QFont>
#include <QColor>
#include <QImage>
#include <QVector>
#include <QRectF>
#include <ghostty/vt.h>

struct TerminalTextCell {
    QString text;QColor fg,bg;
    bool bold=false,italic=false,underline=false,selected=false;
    GhosttyCellWide wide=GHOSTTY_CELL_WIDE_NARROW;
    bool operator==(const TerminalTextCell &other) const {
        return text==other.text && fg==other.fg && bg==other.bg && bold==other.bold &&
            italic==other.italic && underline==other.underline && selected==other.selected && wide==other.wide;
    }
};
// SourceRanges keeps the original scalar renderer as an offline test oracle.
enum class TerminalTextShapingMode { Automatic, SourceRanges };
// Optional offline diagnostics. Raw fonts never leave the GUI-thread rasterizer.
struct TerminalTextPlacement {int firstColumn,lastColumn;quint32 glyph;QPointF position;QString family;bool shaped;bool bulkAscii=false;};
QImage renderTerminalTextRow(const QVector<TerminalTextCell> &,const QFont &,qreal cellWidth,
    qreal cellHeight,qreal ascent,qreal scale,QVector<TerminalTextPlacement> *diagnostics=nullptr,TerminalTextShapingMode mode=TerminalTextShapingMode::Automatic);
// Retained foreground images may omit unwritten trailing columns. Their origin
// and DPR are unchanged; terminal geometry/backgrounds come from the source grid.
class TerminalTextRow {
public:
    bool update(const QVector<TerminalTextCell> &,const QFont &,qreal,qreal,qreal,qreal);
    const QImage &image() const {return image_;}
    quint64 revisions() const {return revisions_;}
private:
    QVector<TerminalTextCell> cells_;QFont font_;QImage image_;
    qreal width_=0,height_=0,ascent_=0,scale_=0;quint64 revisions_=0;
};
bool terminalTextSelfTest(const QFont &);

// Presentation-only invalidation; empty damage must never be passed to Qt's
// update(QRect()), where an empty rectangle requests a full repaint.
struct TerminalTextDamage {
    int firstRow=-1,lastRow=-1;bool full=false;
    bool empty() const {return !full && firstRow<0;}
    QRect rect(const QRectF &bounds,qreal cellHeight) const;
};
// Shared visibility/retention boundary for the live host and CPU benchmark.
class TerminalTextSurface {
public:
    TerminalTextDamage update(const QVector<QVector<TerminalTextCell>> &,const QFont &,qreal,qreal,qreal,qreal,bool presentable);
    int size() const {return rows_.size();}
    const QImage &image(int row) const {return rows_[row].image();}
    quint64 rasterizedRows() const {return rasterizedRows_;}
    qint64 imageBytes() const;
private:
    QVector<TerminalTextRow> rows_;quint64 rasterizedRows_=0;
    QFont font_;qreal cw_=0,ch_=0,ascent_=0,scale_=0;int columns_=0;bool valid_=false;
};

class QPainter;
// Shared pure grid paint path; respects the painter clip and returns rows drawn.
// Caller owns backdrop, cursor and IME overlay.
int paintTerminalGrid(QPainter &,const QVector<QVector<TerminalTextCell>> &,
    const TerminalTextSurface &,qreal cellWidth,qreal cellHeight,const QPointF &origin);
