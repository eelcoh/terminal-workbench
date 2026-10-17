#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint8_t r,g,b;} WorkbenchThemeColor;
typedef struct {
    WorkbenchThemeColor background,foreground,cursor,selection_background,selection_foreground;
    WorkbenchThemeColor palette[256];bool light;
} WorkbenchTheme;
int workbench_theme_count(void);
const char *workbench_theme_name(int index);
bool workbench_theme_get(int index,WorkbenchTheme *result);
#ifdef __cplusplus
}
#endif
