#!/usr/bin/env python3
"""Compile the actual workarea adapter; Wayland must avoid absolute geometry."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('display_patch', ROOT/'scripts/patch-raylib-display.py')
patch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patch)
helper = patch.HELPERS[patch.HELPERS.index('bool WorkbenchWindowWorkarea'):]
prefix = r"""
#include <assert.h>
#include <stdbool.h>
#include <math.h>
#define GLFW_PLATFORM_WAYLAND 1
typedef int GLFWmonitor;
static struct { void *handle; } platform;
static int backend=GLFW_PLATFORM_WAYLAND,calls=0;
static GLFWmonitor monitor;
static int glfwGetPlatform(void) { return backend; }
static GLFWmonitor **glfwGetMonitors(int *count) {
    static GLFWmonitor *monitors[]={&monitor};calls++;*count=1;return monitors;
}
static GLFWmonitor *glfwGetPrimaryMonitor(void) { calls++;return &monitor; }
static void glfwGetWindowPos(void *window,int *x,int *y) {(void)window;calls++;*x=10;*y=10;}
static void glfwGetWindowSize(void *window,int *w,int *h) {(void)window;calls++;*w=800;*h=600;}
static void glfwGetMonitorWorkarea(GLFWmonitor *m,int *x,int *y,int *w,int *h) {
    (void)m;calls++;*x=0;*y=0;*w=1920;*h=1080;
}
static void glfwGetWindowFrameSize(void *window,int *l,int *t,int *r,int *b) {
    (void)window;calls++;*l=1;*t=20;*r=1;*b=1;
}
"""
main = r"""
int main(void) {
    int x=0,y=0,w=0,h=0,l=0,t=0,r=0,b=0,count=99;
    assert(!WorkbenchWindowWorkarea(&x,&y,&w,&h,&l,&t,&r,&b,&count));
    assert(count==0 && calls==0);
    backend=2;
    assert(WorkbenchWindowWorkarea(&x,&y,&w,&h,&l,&t,&r,&b,&count));
    assert(count==1 && calls>0 && w==1920 && h==1080 && t==20);
}
"""
with tempfile.TemporaryDirectory(prefix='wb-wayland-display-') as directory:
    source = Path(directory)/'policy.c'
    source.write_text(prefix+helper+main)
    exe = Path(directory)/'policy'
    subprocess.run([os.environ.get('CC','cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                    str(source), '-lm', '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: Wayland defers absolute geometry to compositor; X11/Mac workarea behavior retained')
