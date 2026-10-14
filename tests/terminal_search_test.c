#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include "terminal_search.h"
static void feed(TerminalSearch *s,const char *text,uint32_t row,uint16_t col)
{for(size_t i=0;text[i];i++) terminal_search_byte(s,(unsigned char)text[i],(TerminalSearchPoint){row,(uint16_t)(col+i),1});}
int main(void)
{
    TerminalSearch s;assert(terminal_search_begin(&s,"aba"));feed(&s,"ababa",2,4);
    assert(s.count==2 && s.matches[0].start.col==4 && s.matches[1].end.col==8);
    assert(terminal_search_move(&s,-1) && s.selected==1);
    s.selected=1;assert(terminal_search_move(&s,1) && s.selected==0);assert(terminal_search_move(&s,-1) && s.selected==1);
    assert(terminal_search_begin(&s,"abcdef"));feed(&s,"abc",0,5);feed(&s,"def",1,0);
    assert(s.count==1 && s.matches[0].start.row==0 && s.matches[0].end.row==1);
    assert(terminal_search_begin(&s,"abcdef"));feed(&s,"abc",0,5);terminal_search_break(&s);feed(&s,"def",1,0);assert(!s.count);
    assert(terminal_search_begin(&s,"界é"));terminal_search_codepoint(&s,0x754c,(TerminalSearchPoint){4,2,2});terminal_search_codepoint(&s,0xe9,(TerminalSearchPoint){4,4,1});
    assert(s.count==1 && s.matches[0].start.col==2 && s.matches[0].end.col==4);
    assert(terminal_search_begin(&s,"e\xcc\x81"));terminal_search_codepoint(&s,'e',(TerminalSearchPoint){3,0,1});terminal_search_codepoint(&s,0x301,(TerminalSearchPoint){3,0,1});assert(s.count==1);
    assert(terminal_search_begin(&s,"💻"));terminal_search_codepoint(&s,0x1f4bb,(TerminalSearchPoint){9,10,2});assert(s.count==1 && s.matches[0].end.width==2);
    assert(terminal_search_begin(&s,"A"));feed(&s,"a",0,0);assert(!s.count);
    assert(terminal_search_begin(&s,""));feed(&s,"anything",0,0);assert(!s.count && !terminal_search_move(&s,1));
    assert(!terminal_search_begin(&s,"bad\nquery") && !terminal_search_begin(&s,"\xc0\x80"));
    assert(terminal_search_begin(&s,"a"));for(int i=0;i<1100;i++)terminal_search_byte(&s,'a',(TerminalSearchPoint){(uint32_t)i,0,1});assert(s.limited && s.count==TERMINAL_SEARCH_MATCHES);
    return 0;
}
