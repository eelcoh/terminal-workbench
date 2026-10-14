/* Pure, bounded UTF-8 single-line editing; offsets are codepoint boundaries. */
#ifndef WORKBENCH_TEXT_EDIT_H
#define WORKBENCH_TEXT_EDIT_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
typedef struct { size_t caret, anchor; } TextEdit;
static inline size_t text_edit_low(TextEdit e) { return e.caret<e.anchor ? e.caret : e.anchor; }
static inline size_t text_edit_high(TextEdit e) { return e.caret>e.anchor ? e.caret : e.anchor; }
static inline bool text_edit_selected(TextEdit e) { return e.caret!=e.anchor; }
static inline void text_edit_init(TextEdit *e,const char *value,bool select)
{ e->caret=strlen(value);e->anchor=select ? 0 : e->caret; }
static inline size_t text_edit_previous(const char *value,size_t offset)
{
    if(!offset) return 0;
    offset--;while(offset && ((unsigned char)value[offset]&0xc0)==0x80) offset--;
    return offset;
}
static inline size_t text_edit_next(const char *value,size_t offset)
{
    if(!value[offset]) return offset;
    offset++;while(((unsigned char)value[offset]&0xc0)==0x80) offset++;
    return offset;
}
static inline bool text_edit_valid(const char *text,size_t size)
{
    const unsigned char *p=(const unsigned char *)text;
    for(size_t i=0;i<size;) {
        uint32_t cp=p[i++],minimum=0;int count=0;
        if(cp<32 || cp==127) return false;
        if(cp<128) continue;
        if(cp>=0xc2 && cp<=0xdf) { cp&=31;count=1;minimum=128; }
        else if(cp>=0xe0 && cp<=0xef) { cp&=15;count=2;minimum=2048; }
        else if(cp>=0xf0 && cp<=0xf4) { cp&=7;count=3;minimum=65536; }
        else return false;
        if(i+(size_t)count>size) return false;
        while(count--) { if((p[i]&0xc0)!=0x80) return false;cp=(cp<<6)|(p[i++]&63); }
        if(cp<minimum || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) return false;
    }
    return true;
}
static inline bool text_edit_replace(TextEdit *e,char *value,size_t capacity,const char *text,size_t size)
{
    size_t length=strlen(value),low=text_edit_low(*e),high=text_edit_high(*e);
    if(high>length || length>=capacity || size>=capacity || length-(high-low)>capacity-1-size || !text_edit_valid(text,size)) return false;
    memmove(value+low+size,value+high,length-high+1);
    if(size) memcpy(value+low,text,size);
    e->caret=e->anchor=low+size;return true;
}
static inline bool text_edit_word_char(const char *text,size_t offset)
{ unsigned char c=(unsigned char)text[offset];return c>=128 || isalnum(c) || c=='_'; }
static inline size_t text_edit_word_left(const char *value,size_t offset)
{
    while(offset && !text_edit_word_char(value,text_edit_previous(value,offset))) offset=text_edit_previous(value,offset);
    while(offset && text_edit_word_char(value,text_edit_previous(value,offset))) offset=text_edit_previous(value,offset);
    return offset;
}
static inline size_t text_edit_word_right(const char *value,size_t offset)
{
    while(value[offset] && !text_edit_word_char(value,offset)) offset=text_edit_next(value,offset);
    while(value[offset] && text_edit_word_char(value,offset)) offset=text_edit_next(value,offset);
    return offset;
}
static inline void text_edit_position(TextEdit *e,size_t offset,bool extend)
{ e->caret=offset;if(!extend) e->anchor=offset; }
static inline void text_edit_move(TextEdit *e,const char *value,int direction,bool word,bool extend)
{
    if(!extend && text_edit_selected(*e)) {
        text_edit_position(e,direction<0 ? text_edit_low(*e) : text_edit_high(*e),false);return;
    }
    size_t offset=word ? (direction<0 ? text_edit_word_left(value,e->caret) : text_edit_word_right(value,e->caret))
        : direction<0 ? text_edit_previous(value,e->caret) : text_edit_next(value,e->caret);
    text_edit_position(e,offset,extend);
}
static inline bool text_edit_delete(TextEdit *e,char *value,size_t capacity,bool backwards,bool word)
{
    if(!text_edit_selected(*e)) {
        e->anchor=backwards ? (word ? text_edit_word_left(value,e->caret) : text_edit_previous(value,e->caret))
            : word ? text_edit_word_right(value,e->caret) : text_edit_next(value,e->caret);
    }
    return text_edit_replace(e,value,capacity,"",0);
}
static inline void text_edit_word_select(TextEdit *e,const char *value,size_t offset)
{
    size_t length=strlen(value);if(offset==length && offset) offset=text_edit_previous(value,offset);
    if(!value[offset]) { text_edit_position(e,offset,false);return; }
    bool word=text_edit_word_char(value,offset);size_t low=offset,high=text_edit_next(value,offset);
    while(low && text_edit_word_char(value,text_edit_previous(value,low))==word) low=text_edit_previous(value,low);
    while(value[high] && text_edit_word_char(value,high)==word) high=text_edit_next(value,high);
    e->anchor=low;e->caret=high;
}
#endif
