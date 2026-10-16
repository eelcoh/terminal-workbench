#include "terminal_bench_fixture.h"
#include <stdexcept>
namespace {
constexpr int columns=80,rows=24;
void require(bool ok,const char *message) {if(!ok)throw std::runtime_error(message);}
}
QVector<QVector<TerminalTextCell>> terminalBenchmarkFixture(bool unicode)
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
    QVector<QVector<TerminalTextCell>> grid;
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
