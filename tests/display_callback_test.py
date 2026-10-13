#!/usr/bin/env python3
"""Compile actual patched GLFW display callbacks with both platform policies."""
from pathlib import Path
import importlib.util,os,subprocess,tempfile,sys
ROOT=Path(__file__).resolve().parent.parent
spec=importlib.util.spec_from_file_location('display_patch',ROOT/'scripts/patch-raylib-display.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
source=Path(sys.argv[1])/'src/platforms/rcore_desktop_glfw.c'
s=source.read_text();assert module.adapt(s)==s
# Recover the unaltered three callback bodies and registration for an idempotence/anchor check.
def function(name):
    start=s.index('static void '+name+'(GLFWwindow *window,')
    while s.find('\n{',start)>s.find(';',start)>=0:
        start=s.index('static void '+name+'(GLFWwindow *window,',start+1)
    end=s.index('\n}',start)+2
    return s[start:end]
bodies='\n'.join(function(n) for n in ['WorkbenchFramebufferSizeCallback','WindowSizeCallback','WindowContentScaleCallback'])
prefix=r'''
#include <stdbool.h>
#include <assert.h>
#include <string.h>
typedef struct { float x,y,z; } Matrix;
typedef struct GLFWwindow { int unused; } GLFWwindow;
static struct { struct { struct {int width,height;} screen,render,currentFbo;Matrix screenScale;bool resizedLastFrame;} Window;} CORE;
static int logicalWidth=1240,logicalHeight=760,backingWidth=1240,backingHeight=760;
static Matrix MatrixIdentity(void) {return (Matrix){1,1,1};}
static Matrix MatrixScale(float x,float y,float z) {return (Matrix){x,y,z};}
static void glfwGetWindowSize(GLFWwindow *w,int *x,int *y) {(void)w;*x=logicalWidth;*y=logicalHeight;}
static void glfwGetFramebufferSize(GLFWwindow *w,int *x,int *y) {(void)w;*x=backingWidth;*y=backingHeight;}
static void SetupViewport(int w,int h) {CORE.Window.render.width=w;CORE.Window.render.height=h;}
'''
main=r'''
int main(void) {
    GLFWwindow window={0};
    for(int density=1;density<=2;density++) {
        backingWidth=logicalWidth*density;backingHeight=logicalHeight*density;
        CORE.Window.resizedLastFrame=false;WindowContentScaleCallback(&window,density,density);
        assert(CORE.Window.screen.width==logicalWidth && CORE.Window.screen.height==logicalHeight);
        assert(CORE.Window.resizedLastFrame);
#ifdef __APPLE__
        assert(CORE.Window.screenScale.x==1 && CORE.Window.screenScale.y==1);
        assert(CORE.Window.render.width==logicalWidth && CORE.Window.render.height==logicalHeight);
#else
        assert(CORE.Window.screenScale.x==density && CORE.Window.screenScale.y==density);
        assert(CORE.Window.render.width==backingWidth && CORE.Window.render.height==backingHeight);
#endif
        WindowSizeCallback(&window,logicalWidth,logicalHeight);
#ifdef __APPLE__
        assert(CORE.Window.screenScale.x==1 && CORE.Window.currentFbo.width==logicalWidth);
#else
        assert(CORE.Window.currentFbo.width==backingWidth);
#endif
    }
    backingWidth=logicalWidth;backingHeight=logicalHeight;
    WindowContentScaleCallback(&window,1,1);assert(CORE.Window.screenScale.x==1);
    int previous=CORE.Window.render.width;
    WorkbenchFramebufferSizeCallback(&window,0,0);assert(CORE.Window.render.width==previous);
}
'''
with tempfile.TemporaryDirectory(prefix='wb-display-callback-') as directory:
    path=Path(directory)/'test.c';path.write_text(prefix+bodies+main)
    for apple in [False,True]:
        exe=Path(directory)/('mac' if apple else 'linux')
        # One platform's matrix constructor is necessarily unused in this extracted fixture.
        subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function',*(['-D__APPLE__'] if apple else []),str(path),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
print('PASS: actual GLFW callbacks keep Mac logical scale at one through 1→2→1, synchronize resize/framebuffer order, preserve Linux backing transforms and ignore minimized buffers')
