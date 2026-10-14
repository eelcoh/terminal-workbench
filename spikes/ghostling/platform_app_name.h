/* Set the standalone viewer identity before GLFW creates the Cocoa app menu. */
#ifndef WORKBENCH_PLATFORM_APP_NAME_H
#define WORKBENCH_PLATFORM_APP_NAME_H
#include <stdlib.h>
static inline void platform_app_name(void)
{
#ifdef __APPLE__
    /* Cocoa's non-bundle menu fallback uses Darwin's program name. Static storage
     * is required: setprogname retains this pointer for the process lifetime. */
    setprogname("Terminal Workbench");
#endif
}
#endif
