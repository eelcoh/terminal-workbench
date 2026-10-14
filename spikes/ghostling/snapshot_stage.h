/* Client-owned staged decoding; the old terminal survives until FINISH validates. */
#ifndef WORKBENCH_SNAPSHOT_STAGE_H
#define WORKBENCH_SNAPSHOT_STAGE_H
#include <ghostty/vt.h>
#include <stdbool.h>
#include <string.h>
typedef struct {
    GhosttySnapshotDecoder decoder;
    GhosttyTerminal terminal;
    size_t length,offset;
    bool finished;
} SnapshotStage;
enum { SNAPSHOT_STAGE_ERROR=-1,SNAPSHOT_STAGE_MORE=0,SNAPSHOT_STAGE_DONE=1 };
static inline void snapshot_stage_cancel(SnapshotStage *stage)
{
    if (stage->decoder) ghostty_snapshot_decoder_free(stage->decoder);
    if (stage->terminal) ghostty_terminal_free(stage->terminal);
    memset(stage,0,sizeof(*stage));
}
static inline bool snapshot_stage_begin(SnapshotStage *stage,const uint8_t *bytes,size_t length)
{
    snapshot_stage_cancel(stage);stage->length=length;size_t continuation=4096;
    if (ghostty_snapshot_decoder_new_buf(NULL,&stage->decoder,bytes,length)!=GHOSTTY_SUCCESS
        || ghostty_snapshot_decoder_set(stage->decoder,GHOSTTY_SNAPSHOT_DECODER_OPT_MAX_CONTINUATION_BYTES,&continuation)!=GHOSTTY_SUCCESS
        || ghostty_snapshot_decoder_ready(stage->decoder,&stage->terminal)!=GHOSTTY_SUCCESS
        || ghostty_snapshot_decoder_get(stage->decoder,GHOSTTY_SNAPSHOT_DECODER_DATA_SOURCE_OFFSET,&stage->offset)!=GHOSTTY_SUCCESS) {
        snapshot_stage_cancel(stage);return false;
    }
    return true;
}
static inline int snapshot_stage_step(SnapshotStage *stage)
{
    if (stage->finished) return SNAPSHOT_STAGE_DONE;
    if (!stage->decoder) return SNAPSHOT_STAGE_ERROR;
    GhosttyResult result=ghostty_snapshot_decoder_next(stage->decoder);
    if ((result!=GHOSTTY_SUCCESS && result!=GHOSTTY_NO_VALUE)
        || ghostty_snapshot_decoder_get(stage->decoder,GHOSTTY_SNAPSHOT_DECODER_DATA_SOURCE_OFFSET,&stage->offset)!=GHOSTTY_SUCCESS
        || stage->offset>stage->length || (result==GHOSTTY_NO_VALUE && stage->offset!=stage->length)) {
        snapshot_stage_cancel(stage);return SNAPSHOT_STAGE_ERROR;
    }
    if (result==GHOSTTY_SUCCESS) return SNAPSHOT_STAGE_MORE;
    ghostty_snapshot_decoder_free(stage->decoder);stage->decoder=NULL;stage->finished=true;
    return SNAPSHOT_STAGE_DONE;
}
static inline GhosttyTerminal snapshot_stage_take(SnapshotStage *stage)
{
    if (!stage->finished) return NULL;
    GhosttyTerminal terminal=stage->terminal;stage->terminal=NULL;return terminal;
}
#endif
