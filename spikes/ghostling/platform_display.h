/* Internal GLFW backend adapter, emitted by the marked Raylib display patch.
 * GetWindowHandle is an NSWindow on Mac, so never cast it to GLFWwindow. */
#include "display_geometry.h"
void WorkbenchFramebufferSize(int *width,int *height);
bool WorkbenchWindowWorkarea(int *x,int *y,int *width,int *height,int *left,int *top,int *right,int *bottom,int *count);
static DisplayWorkarea platform_workarea(void)
{
    DisplayWorkarea work={0};
    if (!WorkbenchWindowWorkarea(&work.area.x,&work.area.y,&work.area.width,&work.area.height,
        &work.left,&work.top,&work.right,&work.bottom,&work.count)) work.count=0;
    return work;
}
