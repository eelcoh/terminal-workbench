/* Workbench's narrow adapter to the same pinned libghostty-vt as the viewer.
 * No PTY, files, platform UI or process ownership lives in this adapter. */
#include <ghostty/vt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define REPLY_LIMIT 65536

typedef struct {
    GhosttyTerminal terminal;
    uint16_t cols, rows;
    bool reply_enabled, overflow;
    size_t reply_len;
    uint8_t reply[REPLY_LIMIT];
} WorkbenchVT;

static void query_reply(GhosttyTerminal terminal, void *userdata, const uint8_t *bytes, size_t length)
{
    (void)terminal;
    WorkbenchVT *state=userdata;
    if (!state->reply_enabled) return;
    if (length>REPLY_LIMIT-state->reply_len) { state->overflow=true; return; }
    memcpy(state->reply+state->reply_len,bytes,length);
    state->reply_len+=length;
}
static bool query_size(GhosttyTerminal terminal, void *userdata, GhosttySizeReportSize *size)
{
    (void)terminal;
    WorkbenchVT *state=userdata;
    *size=(GhosttySizeReportSize){.rows=state->rows,.columns=state->cols};
    /* The current transport does not publish pixel/cell metrics. */
    return true;
}
static bool query_attributes(GhosttyTerminal terminal, void *userdata, GhosttyDeviceAttributes *attributes)
{
    (void)terminal; (void)userdata;
    attributes->primary.conformance_level=GHOSTTY_DA_CONFORMANCE_VT220;
    attributes->primary.features[0]=GHOSTTY_DA_FEATURE_COLUMNS_132;
    attributes->primary.features[1]=GHOSTTY_DA_FEATURE_SELECTIVE_ERASE;
    attributes->primary.features[2]=GHOSTTY_DA_FEATURE_ANSI_COLOR;
    attributes->primary.num_features=3;
    attributes->secondary.device_type=GHOSTTY_DA_DEVICE_TYPE_VT220;
    attributes->secondary.firmware_version=1;
    attributes->secondary.rom_cartridge=0;
    attributes->tertiary.unit_id=0;
    return true;
}
static GhosttyString query_version(GhosttyTerminal terminal, void *userdata)
{
    (void)terminal; (void)userdata;
    static const uint8_t name[]="band";
    return (GhosttyString){.ptr=name,.len=sizeof(name)-1};
}
static bool configure(WorkbenchVT *state)
{
    size_t history_bytes=1024*1024, history_lines=1000, continuation=4096;
    uint64_t images=0;
    return ghostty_terminal_set(state->terminal,GHOSTTY_TERMINAL_OPT_USERDATA,state)==GHOSTTY_SUCCESS
        && ghostty_terminal_set(state->terminal,GHOSTTY_TERMINAL_OPT_WRITE_PTY,(const void *)query_reply)==GHOSTTY_SUCCESS
        && ghostty_terminal_set(state->terminal,GHOSTTY_TERMINAL_OPT_SIZE,(const void *)query_size)==GHOSTTY_SUCCESS
        && ghostty_terminal_set(state->terminal,GHOSTTY_TERMINAL_OPT_DEVICE_ATTRIBUTES,(const void *)query_attributes)==GHOSTTY_SUCCESS
        && ghostty_terminal_set(state->terminal,GHOSTTY_TERMINAL_OPT_XTVERSION,(const void *)query_version)==GHOSTTY_SUCCESS
        && ghostty_terminal_set(state->terminal,GHOSTTY_TERMINAL_OPT_SCROLLBACK_MAX_BYTES,&history_bytes)==GHOSTTY_SUCCESS
        && ghostty_terminal_set(state->terminal,GHOSTTY_TERMINAL_OPT_SCROLLBACK_MAX_LINES,&history_lines)==GHOSTTY_SUCCESS
        && ghostty_terminal_set(state->terminal,GHOSTTY_TERMINAL_OPT_CONTINUATION_MAX_BYTES,&continuation)==GHOSTTY_SUCCESS
        && ghostty_terminal_set(state->terminal,GHOSTTY_TERMINAL_OPT_KITTY_IMAGE_STORAGE_LIMIT,&images)==GHOSTTY_SUCCESS;
}
void wb_vt_free(WorkbenchVT *state)
{
    if (!state) return;
    if (state->terminal) ghostty_terminal_free(state->terminal);
    free(state);
}
WorkbenchVT *wb_vt_new(void)
{
    WorkbenchVT *state=calloc(1,sizeof(*state));
    if (!state) return NULL;
    state->cols=80; state->rows=24;
    if (ghostty_terminal_new(NULL,&state->terminal,state->cols,state->rows)!=GHOSTTY_SUCCESS || !configure(state)) {
        wb_vt_free(state); return NULL;
    }
    return state;
}
int wb_vt_feed(WorkbenchVT *state, const uint8_t *bytes, size_t length, bool replies)
{
    state->reply_len=0; state->overflow=false; state->reply_enabled=replies;
    ghostty_terminal_vt_write(state->terminal,bytes,length);
    state->reply_enabled=false;
    return state->overflow ? -1 : 0;
}
const uint8_t *wb_vt_replies(WorkbenchVT *state, size_t *length)
{
    *length=state->reply_len; return state->reply;
}
int wb_vt_resize(WorkbenchVT *state, uint16_t cols, uint16_t rows)
{
    if (ghostty_terminal_resize(state->terminal,cols,rows,0,0)!=GHOSTTY_SUCCESS) return -1;
    state->cols=cols; state->rows=rows;
    return 0;
}
int wb_vt_export(WorkbenchVT *state, bool binary, uint8_t **bytes, size_t *length)
{
    *bytes=NULL; *length=0;
    if (binary) return ghostty_snapshot_encode_alloc(state->terminal,NULL,bytes,length)==GHOSTTY_SUCCESS ? 0 : -1;
    GhosttyFormatterTerminalOptions options=GHOSTTY_INIT_SIZED(GhosttyFormatterTerminalOptions);
    options.emit=GHOSTTY_FORMATTER_FORMAT_PLAIN; options.trim=true;
    GhosttyFormatter formatter=NULL;
    if (ghostty_formatter_terminal_new(NULL,&formatter,state->terminal,options)!=GHOSTTY_SUCCESS) return -1;
    GhosttyResult result=ghostty_formatter_format_alloc(formatter,NULL,bytes,length);
    ghostty_formatter_free(formatter);
    return result==GHOSTTY_SUCCESS ? 0 : -1;
}
void wb_vt_buffer_free(uint8_t *bytes, size_t length) { ghostty_free(NULL,bytes,length); }
/* Restore is exercised by the runtime tests before snapshot attachment is adopted. */
WorkbenchVT *wb_vt_restore(const uint8_t *bytes, size_t length)
{
    WorkbenchVT *state=calloc(1,sizeof(*state));
    if (!state) return NULL;
    GhosttySnapshotDecoder decoder=NULL;
    if (ghostty_snapshot_decoder_new_buf(NULL,&decoder,bytes,length)!=GHOSTTY_SUCCESS) { free(state); return NULL; }
    GhosttyResult result=ghostty_snapshot_decoder_decode(decoder,&state->terminal);
    ghostty_snapshot_decoder_free(decoder);
    if (result!=GHOSTTY_SUCCESS || !configure(state)
        || ghostty_terminal_get(state->terminal,GHOSTTY_TERMINAL_DATA_COLS,&state->cols)!=GHOSTTY_SUCCESS
        || ghostty_terminal_get(state->terminal,GHOSTTY_TERMINAL_DATA_ROWS,&state->rows)!=GHOSTTY_SUCCESS) {
        wb_vt_free(state); return NULL;
    }
    return state;
}
