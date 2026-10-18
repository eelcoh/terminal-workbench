#pragma once
#include <stdbool.h>
#include <stddef.h>
typedef struct { const char *id; int key; unsigned mods; } WorkbenchShortcut;
#ifdef __cplusplus
extern "C" {
#endif
int workbench_shortcuts_read(const char *,bool,WorkbenchShortcut *,size_t,char *,size_t);
#ifdef __cplusplus
}
#endif
