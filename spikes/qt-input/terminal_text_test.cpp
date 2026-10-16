#include "terminal_text.h"
#include <QFontMetricsF>
#include <QPainter>
#include <cstdio>
#include <cmath>

bool terminalTextSelfTest(const QFont &font)
{
    auto require=[](bool ok,const char *message) {if(!ok)std::fprintf(stderr,"FAIL: grid shaping: %s\n",message);return ok;};
    QFontMetricsF metrics(font);const qreal cw=std::ceil(metrics.horizontalAdvance('M')),ch=std::ceil(metrics.height());
    QVector<TerminalTextCell> cells(8);for(auto &cell:cells) {cell.fg=Qt::white;cell.bg=Qt::black;}
    cells[0].text="A";cells[1].text=QString::fromUtf8("漢");cells[1].wide=GHOSTTY_CELL_WIDE_WIDE;
    cells[2].wide=GHOSTTY_CELL_WIDE_SPACER_TAIL;cells[3].text="B";
    QVector<TerminalTextPlacement> placements;
    auto image=renderTerminalTextRow(cells,font,cw,ch,metrics.ascent(),1,&placements);
    bool sawWide=false,sawB=false;
    for(const auto &p:placements) {if(p.firstColumn==1)sawWide=p.lastColumn==3;if(p.firstColumn==3)sawB=p.lastColumn==4;}
    if(!require(sawWide && sawB,"wide-cell tail does not shift following text"))return false;
    cells[0].text=QString::fromUtf8("👩");cells[1].text=QString::fromUtf8("🏽");cells[1].wide=GHOSTTY_CELL_WIDE_NARROW;
    cells[2].text=QString::fromUtf8("‍💻");cells[2].wide=GHOSTTY_CELL_WIDE_NARROW;cells[3].text="X";
    renderTerminalTextRow(cells,font,cw,ch,metrics.ascent(),1,&placements);
    bool grouped=false;
    for(const auto &p:placements)if(p.firstColumn==0 && p.lastColumn==3)grouped=true;
    if(!require(grouped,"emoji grapheme joins across cells without consuming following column"))return false;
    cells[1].fg=Qt::red;
    renderTerminalTextRow(cells,font,cw,ch,metrics.ascent(),1,&placements);
    for(const auto &p:placements)if(!require(!(p.firstColumn<1 && p.lastColumn>1),"style boundary prevents cross-color shaping"))return false;
    cells[0].text=QString::fromUtf8("م");cells[1].text=QString::fromUtf8("ر");cells[1].fg=Qt::white;cells[2].text="";
    renderTerminalTextRow(cells,font,cw,ch,metrics.ascent(),1,&placements);
    for(const auto &p:placements)if(p.firstColumn<2 && !require(!p.shaped && p.lastColumn==p.firstColumn+1,"RTL retains per-cell order"))return false;
    cells[0].text=QString::fromUcs4(U"𞤀");
    renderTerminalTextRow(cells,font,cw,ch,metrics.ascent(),1,&placements);
    for(const auto &p:placements)if(p.firstColumn==0 && !require(!p.shaped,"supplementary RTL scalar keeps terminal direction policy"))return false;
    TerminalTextRow retained;retained.update(cells,font,cw,ch,metrics.ascent(),1);
    auto revision=retained.revisions();retained.update(cells,font,cw,ch,metrics.ascent(),1);
    if(!require(retained.revisions()==revision,"unchanged row does not reshape or rerasterize"))return false;
    retained.update(cells,font,cw,ch,metrics.ascent(),2);
    if(!require(retained.revisions()==revision+1 && retained.image().devicePixelRatio()==2 && retained.image().height()==ch*2,"scale invalidates row image and backing pixels"))return false;
    cells[0].selected=true;retained.update(cells,font,cw,ch,metrics.ascent(),2);
    if(!require(retained.revisions()==revision+2,"selection invalidates shaping"))return false;
    TerminalTextSurface surface;QVector<QVector<TerminalTextCell>> grid{cells,cells};
    surface.update(grid,font,cw,ch,metrics.ascent(),1,true);
    auto rasterized=surface.rasterizedRows();auto bytes=surface.imageBytes();
    grid[0][0].text="X";surface.update(grid,font,cw,ch,metrics.ascent(),2,false);
    if(!require(surface.rasterizedRows()==rasterized && surface.imageBytes()==bytes,"hidden output/scale keeps cache without rasterization"))return false;
    surface.update(grid,font,cw,ch,metrics.ascent(),2,true);
    if(!require(surface.rasterizedRows()==rasterized+2 && surface.image(0).devicePixelRatio()==2,"reveal catches current data and scale"))return false;
    surface.update(grid,font,cw,ch,metrics.ascent(),2,true);
    if(!require(surface.rasterizedRows()==rasterized+2,"retained surface does no additional rasterization"))return false;
    // Compare cropped retained images on the original full transparent canvas.
    // Include empty rows, written/underlined spaces, trailing wide tails, styles,
    // fractional pixels and changes that grow and shrink the cached extent.
    const QStringList specimens={"", "A", "  ", "office -> != ===", QString::fromUtf8("café 日本語 👩🏽‍💻 नमस्ते مرحبا")};
    TerminalTextRow cropped;
    for(qreal scale:{1.,1.25,1.5,2.})for(const auto &text:specimens)for(int style=0;style<4;++style)for(int tail=0;tail<3;++tail) {
        QVector<TerminalTextCell> row(80);
        for(auto &cell:row) {cell.fg=Qt::white;cell.bg=Qt::black;}
        int index=3;
        for(const auto character:text) {auto &cell=row[index++];cell.text=character;cell.italic=style==1;cell.bold=style==2;
            cell.underline=style==3;cell.selected=style==3;}
        if(tail==1) {row[index].text=QString::fromUtf8("漢");row[index].wide=GHOSTTY_CELL_WIDE_WIDE;
            row[index+1].wide=GHOSTTY_CELL_WIDE_SPACER_TAIL;}
        if(tail==2)row.last().text="X";
        cropped.update(row,font,cw,ch,metrics.ascent(),scale);
        auto expected=renderTerminalTextRow(row,font,cw,ch,metrics.ascent(),scale,nullptr,TerminalTextShapingMode::SourceRanges);
        QImage canvas(expected.size(),expected.format());canvas.setDevicePixelRatio(scale);canvas.fill(Qt::transparent);
        {QPainter painter(&canvas);painter.drawImage(QPointF(0,0),cropped.image());}
        if(!require(canvas==expected,"cropped cache preserves full-row reference pixels"))return false;
        if(!require(cropped.image().width()<=expected.width() && (tail==2 || cropped.image().width()<expected.width()) && cropped.image().devicePixelRatio()==scale,"cropped cache omits only trailing allocation and preserves DPR"))return false;
    }
    QVector<TerminalTextCell> blank(80);cropped.update(blank,font,cw,ch,metrics.ascent(),2);
    if(!require(cropped.image().width()==1 && cropped.image().devicePixelRatio()==2,"empty rows keep a one-pixel DPR-tagged image"))return false;
    // Cropping foreground images must not remove backgrounds on unwritten cells.
    for(qreal scale:{1.,1.25,1.5,2.}) {
        QVector<TerminalTextCell> row(80);for(auto &cell:row) {cell.bg=Qt::black;cell.fg=Qt::white;}
        row[0].text="A";row[79].bg=Qt::blue;
        TerminalTextSurface backgroundSurface;backgroundSurface.update({row,row},font,cw,ch,metrics.ascent(),scale,true);
        QImage actual(qCeil((80*cw+16)*scale),qCeil((2*ch+16)*scale),QImage::Format_ARGB32_Premultiplied);
        actual.setDevicePixelRatio(scale);actual.fill(Qt::red);auto expected=actual.copy();
        {QPainter painter(&actual);paintTerminalGrid(painter,{row,row},backgroundSurface,cw,ch,QPointF(8,8));}
        {QPainter painter(&expected);for(int y=0;y<2;++y) {
            painter.fillRect(QRectF(8,8+y*ch,80*cw,ch),Qt::black);
            painter.fillRect(QRectF(8+79*cw,8+y*ch,cw,ch),Qt::blue);
            painter.drawImage(QPointF(8,8+y*ch),renderTerminalTextRow(row,font,cw,ch,metrics.ascent(),scale));
        }}
        if(!require(actual==expected,"shared grid paint preserves unwritten backgrounds and row/origin clipping"))return false;
    }
    // Compare span backgrounds to the original per-cell compositor at fractional
    // coordinates, with theme/selection color breaks and conservative fallbacks.
    for(qreal scale:{1.,1.25,1.5,2.})for(qreal width:{cw,cw+1})for(int mode=0;mode<8;++mode) {
        QVector<QVector<TerminalTextCell>> grid(3,QVector<TerminalTextCell>(80));
        for(auto &row:grid)for(int x=0;x<row.size();++x) {
            row[x].bg=(x<13 || x>=64)?QColor("#191d24"):x<37?QColor("#a8c7fa"):QColor("#ffcc66");
            if(mode==1)row[x].bg.setAlpha(110);
        }
        TerminalTextSurface empty;
        QImage actual(qCeil((80*width+20)*scale),qCeil((3*ch+20)*scale),QImage::Format_ARGB32_Premultiplied);
        actual.setDevicePixelRatio(scale);actual.fill(QColor("#5f315c"));auto expected=actual.copy();
        const QPointF origin(8.25,7.75);
        const auto configure=[mode](QPainter &painter) {
            if(mode==2)painter.setOpacity(.6);
            if(mode==3)painter.setRenderHint(QPainter::Antialiasing);
            if(mode==4)painter.setCompositionMode(QPainter::CompositionMode_Source);
            if(mode==5)painter.shear(.08,0);
            if(mode==6)painter.rotate(2);
            if(mode==7)painter.setClipRect(QRectF(18.7,10.4,490.3,45.1));
        };
        {QPainter painter(&actual);configure(painter);paintTerminalGrid(painter,grid,empty,width,ch,origin);}
        {QPainter painter(&expected);configure(painter);
            for(int y=0;y<grid.size();++y)for(int x=0;x<grid[y].size();++x)
                painter.fillRect(QRectF(origin.x()+x*width,origin.y()+y*ch,width,ch),grid[y][x].bg);
        }
        if(!require(actual==expected,"background spans preserve per-cell pixels across scales/colors/alpha/opacity/AA/composition/transforms/clipping"))return false;
    }
    // The original source-range mapper is the oracle for the guarded ASCII path.
    const QStringList ascii={"office -> != === <= >= ffi ==> ...", " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~", "---->=====>!!!!=="};
    for(const auto &text:ascii)for(qreal scale:{1.,1.25,1.5,2.})for(int style=0;style<4;++style) {
        QVector<TerminalTextCell> row;
        for(const auto character:text) {TerminalTextCell cell;cell.text=character;cell.fg=Qt::white;cell.bg=Qt::black;
            cell.bold=style==1;cell.italic=style==2;
            if(style==3) {cell.selected=(row.size()/3)%2;cell.fg=cell.selected?Qt::red:Qt::white;cell.underline=cell.selected;}
            row.append(cell);}
        QVector<TerminalTextPlacement> automatic,reference;
        auto fast=renderTerminalTextRow(row,font,cw,ch,metrics.ascent(),scale,&automatic);
        auto slow=renderTerminalTextRow(row,font,cw,ch,metrics.ascent(),scale,&reference,TerminalTextShapingMode::SourceRanges);
        if(!require(fast==slow && automatic.size()==reference.size(),"ASCII optimized pixels/count match source-range oracle"))return false;
        for(int i=0;i<automatic.size();++i) {
            const auto &a=automatic[i];const auto &b=reference[i];
            if(!require(a.firstColumn==b.firstColumn && a.lastColumn==b.lastColumn && a.glyph==b.glyph && a.position==b.position && a.family==b.family,"ASCII glyph positions and ownership match oracle"))return false;
        }
    }
    std::puts("PASS: grid shaping spans, wide tails, emoji, styles, RTL policy and retained/scale/selection invalidation");return true;
}
