#pragma once
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* The default family by name (bundled or installed) and size. */
bool workbench_font_defaults(const char *,bool,char *,size_t,int *,char *,size_t);
#ifdef __cplusplus
}
#endif
