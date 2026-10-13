#!/usr/bin/env python3
"""Marked adaptation of pinned Raylib's GLFW display callbacks (zlib license).

Keep upstream notices. macOS already scales its viewport in SetupViewport;
content-scale callbacks must retain an identity model transform. Framebuffer
callbacks synchronize physical viewport changes that do not resize the window.
"""
from pathlib import Path
import sys
MARKER='// Terminal Workbench alteration: synchronize display backing changes.'
HELPERS=r'''
// Terminal Workbench alteration: synchronize display backing changes.
static void WorkbenchFramebufferSizeCallback(GLFWwindow *window, int width, int height)
{
    int logicalWidth=0,logicalHeight=0;
    glfwGetWindowSize(window,&logicalWidth,&logicalHeight);
    if (width<=0 || height<=0 || logicalWidth<=0 || logicalHeight<=0) return;
    CORE.Window.screen.width=logicalWidth;
    CORE.Window.screen.height=logicalHeight;
#if defined(__APPLE__)
    // Cocoa supplies logical points; SetupViewport already scales to backing pixels.
    CORE.Window.screenScale=MatrixIdentity();
    SetupViewport(logicalWidth,logicalHeight);
#else
    CORE.Window.screenScale=MatrixScale((float)width/logicalWidth,(float)height/logicalHeight,1.0f);
    SetupViewport(width,height);
#endif
    CORE.Window.currentFbo.width=CORE.Window.render.width;
    CORE.Window.currentFbo.height=CORE.Window.render.height;
    CORE.Window.resizedLastFrame=true;
}
void WorkbenchFramebufferSize(int *width,int *height)
{
    glfwGetFramebufferSize(platform.handle,width,height);
}
bool WorkbenchWindowWorkarea(int *x,int *y,int *width,int *height,int *left,int *top,int *right,int *bottom,int *count)
{
    GLFWmonitor **monitors=glfwGetMonitors(count);
    if (!monitors || *count<=0) return false;
    int wx=0,wy=0,ww=0,wh=0;
    glfwGetWindowPos(platform.handle,&wx,&wy);glfwGetWindowSize(platform.handle,&ww,&wh);
    GLFWmonitor *chosen=glfwGetPrimaryMonitor();double largest=0;
    for (int i=0;i<*count;i++) {
        int mx=0,my=0,mw=0,mh=0;glfwGetMonitorWorkarea(monitors[i],&mx,&my,&mw,&mh);
        double overlap=fmax(0,fmin(wx+ww,mx+mw)-fmax(wx,mx))*fmax(0,fmin(wy+wh,my+mh)-fmax(wy,my));
        if (overlap>largest) { largest=overlap;chosen=monitors[i]; }
    }
    if (!chosen) return false;
    glfwGetMonitorWorkarea(chosen,x,y,width,height);
    glfwGetWindowFrameSize(platform.handle,left,top,right,bottom);
    return *width>0 && *height>0;
}
'''
def adapt(text):
    if MARKER in text:
        return adapt_size(text)
    declaration='static void WindowContentScaleCallback(GLFWwindow *window, float scalex, float scaley);'
    assert text.count(declaration)==1
    text=text.replace(declaration,'static void WorkbenchFramebufferSizeCallback(GLFWwindow *window, int width, int height);\n'+declaration)
    anchor='static void WindowSizeCallback(GLFWwindow *window, int width, int height)\n{'
    assert text.count(anchor)==1
    text=text.replace(anchor,HELPERS+'\n'+anchor)
    register='    glfwSetWindowSizeCallback(platform.handle, WindowSizeCallback);'
    assert text.count(register)==1
    text=text.replace(register,'    glfwSetFramebufferSizeCallback(platform.handle, WorkbenchFramebufferSizeCallback);\n'+register)
    old='''static void WindowContentScaleCallback(GLFWwindow *window, float scalex, float scaley)
{
    CORE.Window.screenScale = MatrixScale(scalex, scaley, 1.0f);
}'''
    assert text.count(old)==1
    new='''static void WindowContentScaleCallback(GLFWwindow *window, float scalex, float scaley)
{
    (void)scalex;(void)scaley;
    int width=0,height=0;glfwGetFramebufferSize(window,&width,&height);
    WorkbenchFramebufferSizeCallback(window,width,height);
}'''
    return adapt_size(text.replace(old,new))
def adapt_size(text):
    marker='// Terminal Workbench alteration: use framebuffer metrics on logical resize.'
    if marker in text:return text
    start=text.index('static void WindowSizeCallback(GLFWwindow *window, int width, int height)\n{')
    end=text.index('\n}',start)+2
    replacement='static void WindowSizeCallback(GLFWwindow *window, int width, int height)\n{\n    // Terminal Workbench alteration: use framebuffer metrics on logical resize.\n    (void)width;(void)height;\n    int framebufferWidth=0,framebufferHeight=0;\n    glfwGetFramebufferSize(window,&framebufferWidth,&framebufferHeight);\n    WorkbenchFramebufferSizeCallback(window,framebufferWidth,framebufferHeight);\n}'
    return text[:start]+replacement+text[end:]
def main():
    path=Path(sys.argv[1])/'src/platforms/rcore_desktop_glfw.c'
    original=path.read_text();patched=adapt(original)
    if patched!=original:path.write_text(patched)
if __name__=='__main__':main()
