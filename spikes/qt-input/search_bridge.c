/* Reuse the independently tested, bounded literal UTF-8 scanner. */
#include "search_bridge.h"
#include "../ghostling/terminal_search_vt.h"
#include <stdlib.h>
struct WorkbenchSearch {TerminalSearchScan scan;};
WorkbenchSearch *workbench_search_new(void){return calloc(1,sizeof(WorkbenchSearch));}
void workbench_search_free(WorkbenchSearch *search){free(search);}
bool workbench_search_begin(WorkbenchSearch *s,GhosttyTerminal terminal,const char *query){return terminal_search_scan_begin(&s->scan,terminal,query);}
bool workbench_search_begin_options(WorkbenchSearch *s,GhosttyTerminal terminal,const char *query,bool whole_word,uint32_t (*fold)(uint32_t),bool (*word)(uint32_t)){
    if((whole_word && !word) || !workbench_search_begin(s,terminal,query))return false;
    s->scan.result.whole_word=whole_word;s->scan.result.fold=fold;s->scan.result.word=word;return true;
}
bool workbench_search_step(WorkbenchSearch *s,GhosttyTerminal terminal,size_t budget){terminal_search_scan_step(&s->scan,terminal,budget);return s->scan.scanning;}
void workbench_search_status(WorkbenchSearch *s,int *count,int *selected,bool *limited,bool *failed){
    *count=(int)s->scan.result.count;*selected=s->scan.result.selected;*limited=s->scan.result.limited;*failed=s->scan.failed;
}
bool workbench_search_move(WorkbenchSearch *s,GhosttyTerminal terminal,int direction){
    TerminalSearch *r=&s->scan.result;
    if(!terminal_search_move(r,direction))return false;
    TerminalSearchMatch match=r->matches[r->selected];
    size_t center=s->scan.rows/2;
    GhosttyTerminalScrollViewport scroll={.tag=GHOSTTY_SCROLL_VIEWPORT_ROW};scroll.value.row=match.start.row>center?match.start.row-center:0;
    ghostty_terminal_scroll_viewport(terminal,scroll);return true;
}
bool workbench_search_mask(WorkbenchSearch *s,GhosttyTerminal terminal,unsigned char *mask,size_t size,int *columns,int *rows){
    TerminalSearch *r=&s->scan.result;GhosttyTerminalScrollbar bar={0};
    *columns=s->scan.cols;*rows=s->scan.rows;
    size_t needed=(size_t)(*columns)*(*rows);
    if(size<needed || ghostty_terminal_get(terminal,GHOSTTY_TERMINAL_DATA_SCROLLBAR,&bar)!=GHOSTTY_SUCCESS)return false;
    memset(mask,0,needed);
    for(size_t i=0;i<r->count+1;i++) {
        if(i==r->count && r->selected<0)break;
        TerminalSearchMatch m=r->matches[i==r->count?(size_t)r->selected:i];
        size_t first=m.start.row>bar.offset?m.start.row:bar.offset;
        size_t last=m.end.row<bar.offset+s->scan.rows-1?m.end.row:bar.offset+s->scan.rows-1;
        if(first>last)continue;
        for(size_t row=first;row<=last;row++){
            size_t from=row==m.start.row?m.start.col:0,to=row==m.end.row?m.end.col+m.end.width:s->scan.cols;
            if(to>s->scan.cols)to=s->scan.cols;
            if(from<to)memset(mask+(row-bar.offset)*s->scan.cols+from,i==r->count?2:1,to-from);
        }
    }
    return true;
}
