#pragma once
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
bool workbench_creation_defaults(const char *path,bool apple,char *directory,size_t length,double *width,char *error,size_t capacity);
#ifdef __cplusplus
}
#endif
