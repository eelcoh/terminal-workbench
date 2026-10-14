#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include "terminal_search_vt.h"
static GhosttyTerminal create(int cols,int rows)
{GhosttyTerminal t=NULL;assert(ghostty_terminal_new(NULL,&t,(uint16_t)cols,(uint16_t)rows)==GHOSTTY_SUCCESS);return t;}
static void write_text(GhosttyTerminal t,const char *text) {ghostty_terminal_vt_write(t,(const uint8_t *)text,strlen(text));}
static TerminalSearchScan scan(GhosttyTerminal t,const char *query)
{TerminalSearchScan s;assert(terminal_search_scan_begin(&s,t,query));while(s.scanning){size_t processed=terminal_search_scan_step(&s,t,7);assert(processed<=7 && !s.failed);}return s;}
int main(void)
{
    GhosttyTerminal t=create(8,3);
    write_text(t,"abcdefghi\r\njkl\r\n");
    TerminalSearchScan s=scan(t,"fghi");assert(s.result.count==1 && s.result.matches[0].start.col==5 && s.result.matches[0].end.row==1);
    s=scan(t,"ij");assert(!s.result.count); /* Hard newline cannot join a query. */
    write_text(t,"\x1b[2J\x1b[H界e\xcc\x81💻");
    s=scan(t,"界e\xcc\x81💻");assert(s.result.count==1 && s.result.matches[0].end.col==3 && s.result.matches[0].end.width==2);
    write_text(t,"\x1b[?1049hALT needle");s=scan(t,"needle");assert(s.result.count==1);s=scan(t,"界");assert(!s.result.count);
    write_text(t,"\x1b[?1049l");s=scan(t,"界");assert(s.result.count==1);
    write_text(t,"\x1b[2J\x1b[H");for(int i=0;i<100;i++)write_text(t,"line needle\r\n");
    GhosttyTerminalScrollbar before={0},after={0};ghostty_terminal_get(t,GHOSTTY_TERMINAL_DATA_SCROLLBAR,&before);
    s=scan(t,"needle");assert(s.result.count==100 && s.total>100);
    ghostty_terminal_get(t,GHOSTTY_TERMINAL_DATA_SCROLLBAR,&after);assert(before.offset==after.offset);
    assert(ghostty_terminal_resize(t,5,4,7,16)==GHOSTTY_SUCCESS);s=scan(t,"needle");assert(s.result.count==100);
    GhosttyTrackedGridRef bookmark=NULL;GhosttyPoint top={.tag=GHOSTTY_POINT_TAG_VIEWPORT,.value.coordinate={0,0}};
    assert(ghostty_terminal_grid_ref_track(t,top,&bookmark)==GHOSTTY_SUCCESS);
    ghostty_terminal_free(t);assert(!ghostty_tracked_grid_ref_has_value(bookmark));ghostty_tracked_grid_ref_free(bookmark);
    puts("PASS: actual Ghostty scrollback, soft-wrap/newline boundaries, Unicode cells, alternate-screen isolation, reflow and read-only scans");return 0;
}
