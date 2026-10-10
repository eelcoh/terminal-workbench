#!/usr/bin/env python3
"""Compile the actual adapted Raylib callback/query bodies without a display."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile


def function(text, signature):
    start=text.index(signature+'\n{'); opening=text.index('{',start); depth=1; end=opening+1
    while depth:
        depth += (text[end]=='{') - (text[end]=='}'); end+=1
    return text[start:end]


def main():
    source=Path(sys.argv[1])/'src'
    core=(source/'rcore.c').read_text(); glfw=(source/'platforms/rcore_desktop_glfw.c').read_text()
    callback=function(glfw,'static void MouseButtonCallback(GLFWwindow *window, int button, int action, int mods)')
    queries='\n'.join(function(core,'bool '+name+'(int button)') for name in ('IsMouseButtonPressed','IsMouseButtonReleased','IsMouseButtonDown'))
    # Use the backend's actual per-poll reset, not a duplicate test implementation.
    if 'workbenchPressed[i] = false;' in glfw:
        reset_start=glfw.index('    for (int i = 0; i < MAX_MOUSE_BUTTONS; i++) {')
        reset_end=glfw.index('\n    }',reset_start)+6
        reset=glfw[reset_start:reset_end]
    else:
        reset=next(line for line in glfw.splitlines() if 'for (int i = 0; i < MAX_MOUSE_BUTTONS; i++) CORE.Input.Mouse.previousButtonState' in line)
    touch_reset = next(line for line in glfw.splitlines() if 'for (int i = 0; i < MAX_TOUCH_POINTS; i++) CORE.Input.Touch.previousTouchState' in line)
    reset += '\n' + touch_reset
    fixture = '''#include <stdbool.h>
#include <assert.h>
#define MAX_MOUSE_BUTTONS 8
#define MAX_TOUCH_POINTS 8
#define GLFW_PRESS 1
#define GLFW_RELEASE 0
typedef struct GLFWwindow GLFWwindow;
static struct { struct {
    struct { char currentButtonState[8], previousButtonState[8]; bool workbenchPressed[8], workbenchReleased[8]; } Mouse;
    struct { char currentTouchState[8], previousTouchState[8]; } Touch;
} Input; } CORE;
'''
    assertions = '''
int main(void) {
    poll_reset();
    MouseButtonCallback(0,0,GLFW_PRESS,0);
    MouseButtonCallback(0,0,GLFW_RELEASE,0);
    assert(IsMouseButtonPressed(0) && IsMouseButtonReleased(0) && !IsMouseButtonDown(0));
    assert(IsMouseButtonPressed(0) && IsMouseButtonReleased(0)); // queries don't consume edges
    assert(!IsMouseButtonPressed(1) && !IsMouseButtonReleased(1));
    poll_reset();
    assert(!IsMouseButtonPressed(0) && !IsMouseButtonReleased(0) && !IsMouseButtonDown(0));
    MouseButtonCallback(0,1,GLFW_PRESS,0);
    assert(IsMouseButtonPressed(1) && !IsMouseButtonReleased(1) && IsMouseButtonDown(1));
    poll_reset();
    assert(!IsMouseButtonPressed(1) && !IsMouseButtonReleased(1) && IsMouseButtonDown(1));
    MouseButtonCallback(0,1,GLFW_RELEASE,0);
    assert(!IsMouseButtonPressed(1) && IsMouseButtonReleased(1) && !IsMouseButtonDown(1));
    poll_reset();
    assert(!IsMouseButtonReleased(1));
    MouseButtonCallback(0,2,GLFW_PRESS,0);
    poll_reset();
    MouseButtonCallback(0,2,GLFW_RELEASE,0);
    MouseButtonCallback(0,2,GLFW_PRESS,0);
    assert(IsMouseButtonPressed(2) && IsMouseButtonReleased(2) && IsMouseButtonDown(2));
    poll_reset();
    assert(IsMouseButtonDown(2) && !IsMouseButtonPressed(2) && !IsMouseButtonReleased(2));
}
'''
    with tempfile.TemporaryDirectory(prefix='wb-mouse-edges-') as directory:
        path=Path(directory)/'edges.c'; binary=Path(directory)/'edges'
        path.write_text(fixture+callback+'\n'+queries+'\nstatic void poll_reset(void) {\n'+reset+'\n}\n'+assertions)
        subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter',str(path),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
    print('PASS: actual Raylib callback retains same-poll press/release, queries are repeatable, edges expire, held/released state and independent buttons remain correct')


if __name__=='__main__': main()
