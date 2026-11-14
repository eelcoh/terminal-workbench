#pragma once
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
bool workbench_view_defaults(const char *path,bool apple,bool *pinned,bool *motion,bool *title,bool *dim,bool *material,char *error,size_t capacity);
#ifdef __cplusplus
}
#endif
