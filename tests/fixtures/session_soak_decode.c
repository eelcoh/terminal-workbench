/* Decode captured snapshots through the actual pinned runtime adapter. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct WorkbenchVT WorkbenchVT;
WorkbenchVT *wb_vt_restore(const uint8_t *, size_t);
int wb_vt_export(WorkbenchVT *, bool, uint8_t **, size_t *);
void wb_vt_buffer_free(uint8_t *, size_t);
void wb_vt_free(WorkbenchVT *);
int main(void) {
    const size_t limit=64*1024*1024;
    uint8_t *snapshot=malloc(limit);
    if (!snapshot) return 1;
    size_t length=fread(snapshot,1,limit,stdin);
    if (ferror(stdin) || (length==limit && fgetc(stdin)!=EOF)) { free(snapshot);return 1; }
    WorkbenchVT *terminal=wb_vt_restore(snapshot,length);
    free(snapshot);
    if (!terminal) return 2;
    uint8_t *text=NULL;size_t text_length=0;
    int result=wb_vt_export(terminal,false,&text,&text_length);
    if (!result && text_length && fwrite(text,1,text_length,stdout)!=text_length) result=-1;
    wb_vt_buffer_free(text,text_length);wb_vt_free(terminal);
    return result ? 3 : 0;
}
