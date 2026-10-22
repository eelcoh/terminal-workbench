/* Incremental read-only scan of the viewer's retained Ghostty active screen. */
#ifndef WORKBENCH_TERMINAL_SEARCH_VT_H
#define WORKBENCH_TERMINAL_SEARCH_VT_H
#include "terminal_search.h"
#include <ghostty/vt.h>
typedef struct {
    TerminalSearch result;
    size_t row,total,pending;
    uint16_t col,cols,rows,pending_col;
    bool scanning,failed;
} TerminalSearchScan;
static inline GhosttyPoint terminal_search_point(uint32_t row,uint16_t col)
{ return (GhosttyPoint){.tag=GHOSTTY_POINT_TAG_SCREEN,.value.coordinate={col,row}}; }
static inline bool terminal_search_scan_begin(TerminalSearchScan *scan,GhosttyTerminal terminal,const char *query)
{
    memset(scan,0,sizeof(*scan));
    if(!terminal_search_begin(&scan->result,query)
        || ghostty_terminal_get(terminal,GHOSTTY_TERMINAL_DATA_TOTAL_ROWS,&scan->total)!=GHOSTTY_SUCCESS
        || ghostty_terminal_get(terminal,GHOSTTY_TERMINAL_DATA_COLS,&scan->cols)!=GHOSTTY_SUCCESS
        || ghostty_terminal_get(terminal,GHOSTTY_TERMINAL_DATA_ROWS,&scan->rows)!=GHOSTTY_SUCCESS
        || !scan->cols || scan->cols>1000 || !scan->rows || scan->rows>1000 || scan->total>UINT32_MAX) {
        scan->failed=true;return false;
    }
    scan->scanning=scan->result.length>0 && scan->total>0;return true;
}
static inline void terminal_search_pending(TerminalSearchScan *scan)
{
    for(size_t i=0;i<scan->pending;i++) terminal_search_codepoint(&scan->result,' ',(TerminalSearchPoint){(uint32_t)scan->row,(uint16_t)(scan->pending_col+i),1});
    scan->pending=0;
}
static inline size_t terminal_search_scan_step(TerminalSearchScan *scan,GhosttyTerminal terminal,size_t budget)
{
    size_t processed=0;
    while(scan->scanning && processed<budget) {
        GhosttyGridRef ref=GHOSTTY_INIT_SIZED(GhosttyGridRef);
        GhosttyRow row;bool wrapped=false;
        if(ghostty_terminal_grid_ref(terminal,terminal_search_point((uint32_t)scan->row,0),&ref)!=GHOSTTY_SUCCESS
            || ghostty_grid_ref_row(&ref,&row)!=GHOSTTY_SUCCESS
            || ghostty_row_get(row,GHOSTTY_ROW_DATA_WRAP,&wrapped)!=GHOSTTY_SUCCESS) goto failed;
        while(scan->col<scan->cols && processed<budget && !scan->result.limited) {
            ref.x=scan->col;GhosttyCell cell;GhosttyCellWide wide;uint32_t cp;
            if(ghostty_grid_ref_cell(&ref,&cell)!=GHOSTTY_SUCCESS
                || ghostty_cell_get(cell,GHOSTTY_CELL_DATA_WIDE,&wide)!=GHOSTTY_SUCCESS
                || ghostty_cell_get(cell,GHOSTTY_CELL_DATA_CODEPOINT,&cp)!=GHOSTTY_SUCCESS) goto failed;
            uint16_t col=scan->col++;processed++;
            if(wide==GHOSTTY_CELL_WIDE_SPACER_HEAD || wide==GHOSTTY_CELL_WIDE_SPACER_TAIL) continue;
            if(!cp) {if(!scan->pending) scan->pending_col=col;scan->pending++;continue;}
            terminal_search_pending(scan);
            uint32_t points[256];size_t count=0;
            if(ghostty_grid_ref_graphemes(&ref,points,256,&count)!=GHOSTTY_SUCCESS) goto failed;
            TerminalSearchPoint point={(uint32_t)scan->row,col,wide==GHOSTTY_CELL_WIDE_WIDE ? 2 : 1};
            for(size_t i=0;i<count;i++) terminal_search_codepoint(&scan->result,points[i],point);
        }
        if(scan->result.limited) {scan->scanning=false;break;}
        if(scan->col==scan->cols) {
            if(wrapped) terminal_search_pending(scan);
            else {scan->pending=0;terminal_search_break(&scan->result);}
            scan->col=0;scan->row++;
            if(scan->row==scan->total) {terminal_search_finish(&scan->result,false);scan->scanning=false;}
        }
    }
    return processed;
failed:
    scan->failed=true;scan->scanning=false;scan->result.count=0;scan->result.selected=-1;return processed;
}
#endif
