#!/usr/bin/env python3
"""Marked callback-order bridge for pinned Raylib/GLFW; preserve upstream zlib notices."""
from pathlib import Path
import sys

MARKER = '// Terminal Workbench alteration: ordered terminal keyboard events.\n'

def adapt_layout(text):
    old='    wb_keyboard_push_key(key, action, mods);'
    new='    wb_keyboard_push_key(key, action, mods, wb_keyboard_layout_codepoint(glfwGetKeyName(key, scancode)));'
    if new in text:return text
    assert text.count(old)==1, 'Pinned Raylib layout keyboard anchor changed'
    return text.replace(old,new)

def adapt(text):
    if MARKER in text:return adapt_layout(text)
    edits = {
        '    CORE.Input.Keyboard.keyPressedQueueCount = 0;':
            '    wb_keyboard_reset();\n    CORE.Input.Keyboard.keyPressedQueueCount = 0;',
        'static void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)\n{':
            'static void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)\n{\n    wb_keyboard_push_key(key, action, mods);',
        'static void CharCallback(GLFWwindow *window, unsigned int codepoint)\n{':
            'static void CharCallback(GLFWwindow *window, unsigned int codepoint)\n{\n    wb_keyboard_push_text(codepoint);',
        'static void WindowFocusCallback(GLFWwindow *window, int focused)\n{':
            'static void WindowFocusCallback(GLFWwindow *window, int focused)\n{\n    if (!focused) wb_keyboard_reset();',
    }
    for old, new in edits.items():
        assert text.count(old)==1, 'Pinned Raylib keyboard anchor changed: '+old
        text=text.replace(old,new)
    return adapt_layout(MARKER+'#define WB_KEYBOARD_IMPLEMENTATION\n#include "workbench_keyboard_events.h"\n'+text)

def main():
    source=Path(sys.argv[1])/'src/platforms'
    header=Path(__file__).with_name('platform_keyboard.h')
    target=source/'workbench_keyboard_events.h'
    if not target.exists() or target.read_bytes()!=header.read_bytes():target.write_bytes(header.read_bytes())
    path=source/'rcore_desktop_glfw.c';text=path.read_text()
    patched=adapt(text)
    if patched!=text:path.write_text(patched)

if __name__=='__main__':main()
