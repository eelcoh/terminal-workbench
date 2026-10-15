#!/usr/bin/env python3
"""Compile the adapted GLFW Wayland enter callback and exercise cursor modes."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

source=Path(sys.argv[1])/'src/external/glfw/src/wl_window.c'
text=source.read_text()
start=text.index('static void pointerHandleEnter(')
opening=text.index('{',start);end=opening+1;depth=1
while depth:
    depth+=(text[end]=='{')-(text[end]=='}');end+=1
callback=text[start:end]
prefix=r'''
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#define GLFW_TRUE 1
#define GLFW_CURSOR_DISABLED 2
typedef int32_t wl_fixed_t;
struct wl_pointer {int unused;};
struct wl_surface;
struct wl_proxy;
typedef struct {
    int cursorMode;
    struct {bool hovered;int currentCursor;double cursorPosX,cursorPosY;
        struct wl_surface *surface;
        struct {int decorations;struct wl_surface *focus;} fallback;
    } wl;
} _GLFWwindow;
struct wl_surface {bool tagged;_GLFWwindow *window;};
static struct {struct {uint32_t serial,pointerEnterSerial;int tag;_GLFWwindow *pointerFocus;} wl;} _glfw;
static int enters,positions;static double x,y;
static const void *wl_proxy_get_tag(struct wl_proxy *proxy) {struct wl_surface *s=(struct wl_surface *)proxy;return s->tagged?&_glfw.wl.tag:0;}
static _GLFWwindow *wl_surface_get_user_data(struct wl_surface *s) {return s->window;}
static double wl_fixed_to_double(wl_fixed_t value) {return value/256.0;}
static void _glfwSetCursorWayland(_GLFWwindow *w,int cursor) {(void)w;(void)cursor;}
static void _glfwInputCursorEnter(_GLFWwindow *w,int entered) {(void)w;assert(entered);enters++;}
static void _glfwInputCursorPos(_GLFWwindow *w,double a,double b) {(void)w;positions++;x=a;y=b;}
'''
main=r'''
int main(void) {
    _GLFWwindow window={0};
    struct wl_surface surface={true,&window},other={true,&window},foreign={false,&window};
    window.wl.surface=&surface;
    pointerHandleEnter(0,0,1,0,0,0);pointerHandleEnter(0,0,2,&foreign,0,0);
    assert(enters==0 && positions==0 && _glfw.wl.pointerFocus==0);
    pointerHandleEnter(0,0,7,&surface,40512,56448);
    assert(enters==1 && positions==1 && window.wl.hovered);
    assert(x==158.25 && y==220.5 && window.wl.cursorPosX==x && window.wl.cursorPosY==y);
    assert(_glfw.wl.serial==7 && _glfw.wl.pointerEnterSerial==7 && _glfw.wl.pointerFocus==&window);
    window.cursorMode=GLFW_CURSOR_DISABLED;
    pointerHandleEnter(0,0,8,&surface,256,512);
    assert(enters==2 && positions==1 && x==158.25 && y==220.5);
    window.wl.fallback.decorations=1;
    pointerHandleEnter(0,0,9,&other,768,1024);
    assert(window.wl.fallback.focus==&other && positions==1 && enters==2);
}
'''
with tempfile.TemporaryDirectory(prefix='wb-pointer-enter-') as directory:
    path=Path(directory)/'test.c';exe=Path(directory)/'test'
    path.write_text(prefix+callback+main)
    # Protocol callbacks retain unused upstream parameters.
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter',str(path),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: Wayland enter publishes logical coordinates before first click, preserves disabled-cursor events and ignores foreign/decorative surfaces')
