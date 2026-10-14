#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "snapshot_stage.h"
static void write_text(GhosttyTerminal terminal,const char *text)
{ghostty_terminal_vt_write(terminal,(const uint8_t *)text,strlen(text));}
static char *format(GhosttyTerminal terminal)
{
    GhosttyFormatterTerminalOptions options=GHOSTTY_INIT_SIZED(GhosttyFormatterTerminalOptions);
    options.emit=GHOSTTY_FORMATTER_FORMAT_PLAIN;options.trim=true;
    GhosttyFormatter formatter=NULL;uint8_t *bytes=NULL;size_t length=0;
    assert(ghostty_formatter_terminal_new(NULL,&formatter,terminal,options)==GHOSTTY_SUCCESS);
    assert(ghostty_formatter_format_alloc(formatter,NULL,&bytes,&length)==GHOSTTY_SUCCESS);
    char *text=malloc(length+1);assert(text);memcpy(text,bytes,length);text[length]=0;
    ghostty_free(NULL,bytes,length);ghostty_formatter_free(formatter);return text;
}
int main(void)
{
    GhosttyTerminal source=NULL,old=NULL;
    assert(ghostty_terminal_new(NULL,&source,80,24)==GHOSTTY_SUCCESS);
    size_t continuation=4096;assert(ghostty_terminal_set(source,GHOSTTY_TERMINAL_OPT_CONTINUATION_MAX_BYTES,&continuation)==GHOSTTY_SUCCESS);
    assert(ghostty_terminal_new(NULL,&old,80,24)==GHOSTTY_SUCCESS);write_text(old,"OLD_TERMINAL");
    for (int row=0;row<900;row++) {char text[96];snprintf(text,sizeof(text),"history-%d-needle\r\n",row);write_text(source,text);}
    write_text(source,"\x1b[?1049h\x1b[2J\x1b[HSTAGED_ALTERNATE\x1b[31");
    uint8_t *snapshot=NULL;size_t length=0;
    assert(ghostty_snapshot_encode_alloc(source,NULL,&snapshot,&length)==GHOSTTY_SUCCESS);
    SnapshotStage stage={0};assert(snapshot_stage_begin(&stage,snapshot,length));
    assert(!snapshot_stage_take(&stage));size_t previous=stage.offset;int count=0,result;
    do {result=snapshot_stage_step(&stage);assert(result>=0 && stage.offset>=previous);previous=stage.offset;count++;} while(result==SNAPSHOT_STAGE_MORE);
    assert(count>1 && stage.finished && !stage.decoder);
    GhosttyTerminal restored=snapshot_stage_take(&stage);assert(restored);
    char *text=format(restored);assert(strstr(text,"STAGED_ALTERNATE"));free(text);
    write_text(restored,"m LIVE_AFTER_STAGE\x1b[0m\x1b[?1049l");text=format(restored);assert(strstr(text,"history-899-needle"));free(text);
    ghostty_terminal_free(restored);snapshot_stage_cancel(&stage);
    assert(snapshot_stage_begin(&stage,snapshot,length));snapshot_stage_cancel(&stage);assert(!stage.decoder && !stage.terminal);
    uint8_t *corrupt=malloc(length+1);assert(corrupt);memcpy(corrupt,snapshot,length);corrupt[length-1]^=1;
    assert(snapshot_stage_begin(&stage,corrupt,length));
    do {result=snapshot_stage_step(&stage);} while(result==SNAPSHOT_STAGE_MORE);
    assert(result==SNAPSHOT_STAGE_ERROR && !snapshot_stage_take(&stage));
    text=format(old);assert(strstr(text,"OLD_TERMINAL"));free(text);
    memcpy(corrupt,snapshot,length);corrupt[length]=0;
    assert(snapshot_stage_begin(&stage,corrupt,length+1));
    do {result=snapshot_stage_step(&stage);} while(result==SNAPSHOT_STAGE_MORE);
    assert(result==SNAPSHOT_STAGE_ERROR);
    snapshot_stage_cancel(&stage);free(corrupt);ghostty_free(NULL,snapshot,length);
    ghostty_terminal_free(source);ghostty_terminal_free(old);
    puts("PASS: page-by-page decoding, complete-checksum commit, parser continuation, both screens, cancellation and corrupt/trailing-byte isolation");
    return 0;
}
