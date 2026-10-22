#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <ghostty/vt.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct WorkbenchSearch WorkbenchSearch;
WorkbenchSearch *workbench_search_new(void);
void workbench_search_free(WorkbenchSearch *);
bool workbench_search_begin(WorkbenchSearch *,GhosttyTerminal,const char *);
bool workbench_search_begin_options(WorkbenchSearch *,GhosttyTerminal,const char *,bool,uint32_t (*)(uint32_t),bool (*)(uint32_t));
bool workbench_search_step(WorkbenchSearch *,GhosttyTerminal,size_t);
void workbench_search_status(WorkbenchSearch *,int *,int *,bool *,bool *);
bool workbench_search_move(WorkbenchSearch *,GhosttyTerminal,int);
bool workbench_search_mask(WorkbenchSearch *,GhosttyTerminal,unsigned char *,size_t,int *,int *);
#ifdef __cplusplus
}
#endif
