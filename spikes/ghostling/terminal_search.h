/* Literal UTF-8 matching with cell coordinates, independent of terminal/UI APIs. */
#ifndef WORKBENCH_TERMINAL_SEARCH_H
#define WORKBENCH_TERMINAL_SEARCH_H
#include "text_edit.h"
#include <stdint.h>
#define TERMINAL_SEARCH_QUERY 256
#define TERMINAL_SEARCH_MATCHES 1024
typedef struct { uint32_t row; uint16_t col,width; } TerminalSearchPoint;
typedef struct { TerminalSearchPoint start,end; } TerminalSearchMatch;
typedef struct {
    unsigned char query[TERMINAL_SEARCH_QUERY];
    size_t prefix[TERMINAL_SEARCH_QUERY],length,matched,seen,count;
    TerminalSearchPoint ring[TERMINAL_SEARCH_QUERY];
    TerminalSearchMatch matches[TERMINAL_SEARCH_MATCHES];
    int selected;
    bool limited,whole_word,previous_word,pending_match;
    /* Left boundary for each byte in the existing position ring. */
    bool before[TERMINAL_SEARCH_QUERY];
    TerminalSearchMatch pending;
    /* Optional codepoint callbacks; queries must use the same simple fold. */
    uint32_t (*fold)(uint32_t);
    bool (*word)(uint32_t);
} TerminalSearch;
static inline bool terminal_search_begin(TerminalSearch *search,const char *query)
{
    size_t length=strnlen(query,TERMINAL_SEARCH_QUERY);
    if(length>=TERMINAL_SEARCH_QUERY || !text_edit_valid(query,length)) return false;
    memset(search,0,sizeof(*search));search->selected=-1;search->length=length;
    memcpy(search->query,query,length);
    for(size_t i=1,j=0;i<length;i++) {
        while(j && search->query[i]!=search->query[j]) j=search->prefix[j-1];
        if(search->query[i]==search->query[j]) j++;
        search->prefix[i]=j;
    }
    return true;
}
static inline void terminal_search_record(TerminalSearch *search,TerminalSearchMatch match)
{
    if(search->limited) return;
    search->matches[search->count++]=match;
    if(search->count==TERMINAL_SEARCH_MATCHES) search->limited=true;
}
static inline void terminal_search_finish(TerminalSearch *search,bool next_word)
{
    if(search->pending_match && !next_word) terminal_search_record(search,search->pending);
    search->pending_match=false;
}
static inline void terminal_search_break(TerminalSearch *search)
{terminal_search_finish(search,false);search->matched=0;search->previous_word=false;}
static inline void terminal_search_byte(TerminalSearch *search,unsigned char byte,TerminalSearchPoint point)
{
    if(!search->length || search->limited) return;
    search->ring[search->seen%TERMINAL_SEARCH_QUERY]=point;
    if(search->whole_word) search->before[search->seen%TERMINAL_SEARCH_QUERY]=search->previous_word;
    search->seen++;
    while(search->matched && byte!=search->query[search->matched]) search->matched=search->prefix[search->matched-1];
    if(byte==search->query[search->matched]) search->matched++;
    if(search->matched==search->length) {
        const size_t first=(search->seen-search->length)%TERMINAL_SEARCH_QUERY;
        const TerminalSearchMatch match={search->ring[first],point};
        if(!search->whole_word) terminal_search_record(search,match);
        else if(!search->before[first]) {search->pending=match;search->pending_match=true;}
        search->matched=search->prefix[search->matched-1];
    }
}
static inline void terminal_search_codepoint(TerminalSearch *search,uint32_t cp,TerminalSearchPoint point)
{
    const bool word=search->whole_word && search->word && search->word(cp);
    if(search->whole_word) terminal_search_finish(search,word);
    if(search->limited) return;
    if(search->fold) cp=search->fold(cp);
    unsigned char bytes[4];int count=0;
    if(cp<0x80) {bytes[0]=(unsigned char)cp;count=1;}
    else if(cp<0x800) {bytes[0]=0xc0|(cp>>6);bytes[1]=0x80|(cp&63);count=2;}
    else if(cp<0x10000 && !(cp>=0xd800 && cp<=0xdfff)) {bytes[0]=0xe0|(cp>>12);bytes[1]=0x80|((cp>>6)&63);bytes[2]=0x80|(cp&63);count=3;}
    else if(cp>=0x10000 && cp<=0x10ffff) {bytes[0]=0xf0|(cp>>18);bytes[1]=0x80|((cp>>12)&63);bytes[2]=0x80|((cp>>6)&63);bytes[3]=0x80|(cp&63);count=4;}
    else {terminal_search_break(search);return;}
    for(int i=0;i<count;i++) terminal_search_byte(search,bytes[i],point);
    search->previous_word=word;
}
static inline bool terminal_search_move(TerminalSearch *search,int direction)
{
    if(!search->count) return false;
    if(search->selected<0 || search->selected>=(int)search->count) search->selected=direction<0 ? (int)search->count-1 : 0;
    else search->selected=(search->selected+direction+(int)search->count)%(int)search->count;
    return true;
}
#endif
