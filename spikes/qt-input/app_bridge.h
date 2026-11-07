#pragma once
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { char id[41],name[64],command[1024],directory[1024],install[1024],setup[1024]; } WorkbenchApp;
/* Fills up to capacity definitions for this platform; returns the count, or -1 with error set. */
int workbench_app_definitions(const char *path,bool apple,WorkbenchApp *out,int capacity,char *error,size_t length);
typedef struct { char id[35],name[64],destination[256],directory[1024],theme[128]; } WorkbenchHost;
/* SSH hosts for this platform, as for apps. */
int workbench_host_definitions(const char *path,bool apple,WorkbenchHost *out,int capacity,char *error,size_t length);
#ifdef __cplusplus
}
#endif
