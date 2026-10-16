#pragma once
#include <QFont>
#include <QColor>
#include <QImage>
#include <QVector>
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
// Optional offline diagnostics. Raw fonts never leave the GUI-thread rasterizer.
struct TerminalTextPlacement {int firstColumn,lastColumn;quint32 glyph;QPointF position;QString family;bool shaped;};
QImage renderTerminalTextRow(const QVector<TerminalTextCell> &,const QFont &,qreal cellWidth,
    qreal cellHeight,qreal ascent,qreal scale,QVector<TerminalTextPlacement> *diagnostics=nullptr);
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

// Shared visibility/retention boundary for the live host and CPU benchmark.
class TerminalTextSurface {
public:
    void update(const QVector<QVector<TerminalTextCell>> &,const QFont &,qreal,qreal,qreal,qreal,bool presentable);
    int size() const {return rows_.size();}
    const QImage &image(int row) const {return rows_[row].image();}
    quint64 rasterizedRows() const {return rasterizedRows_;}
    qint64 imageBytes() const;
private:
    QVector<TerminalTextRow> rows_;quint64 rasterizedRows_=0;
};
