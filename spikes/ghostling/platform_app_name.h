/* Set the standalone viewer identity before GLFW creates the Cocoa app menu. */
#ifndef WORKBENCH_PLATFORM_APP_NAME_H
#define WORKBENCH_PLATFORM_APP_NAME_H
#include <stdlib.h>
static inline void platform_app_name(void)
{
#ifdef __APPLE__
    /* Retain a process-name fallback. Cocoa's authoritative display name now
     * comes from the embedded macos-Info.plist; binary paths stay compatible. */
    setprogname("Terminal Workbench");
#endif
}
#endif
