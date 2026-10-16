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
    if(!require(retained.revisions()==revision+1 && retained.image().width()==cells.size()*cw*2,"scale invalidates row image and backing pixels"))return false;
    cells[0].selected=true;retained.update(cells,font,cw,ch,metrics.ascent(),2);
    if(!require(retained.revisions()==revision+2,"selection invalidates shaping"))return false;
    std::puts("PASS: grid shaping spans, wide tails, emoji, styles, RTL policy and retained/scale/selection invalidation");return true;
}
