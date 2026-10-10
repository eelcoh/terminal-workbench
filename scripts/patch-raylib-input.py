#!/usr/bin/env python3
"""Marked, bounded adaptation of Raylib 5.5's GLFW mouse edges (zlib license).

Keep upstream copyright/license notices intact. Reapply idempotently on configure;
fail visibly if pinned source anchors change. No synthetic held-button state.
"""
from pathlib import Path
import sys

MARKER = "// Terminal Workbench alteration: retain mouse press/release edges within one poll.\n"


def adapt(core, glfw):
    fields = "            char previousButtonState[MAX_MOUSE_BUTTONS];"
    assert core.count(fields) == 1, "Raylib mouse state anchor changed"
    core = core.replace(fields, "            bool workbenchPressed[MAX_MOUSE_BUTTONS];\n            bool workbenchReleased[MAX_MOUSE_BUTTONS];\n" + fields)
    for query, flag in [('bool IsMouseButtonPressed(int button)', 'workbenchPressed'),
                        ('bool IsMouseButtonReleased(int button)', 'workbenchReleased')]:
        start=core.index(query)
        end=core.index('\n}',start)
        body=core[start:end]
        anchor='    return '+('pressed' if flag=='workbenchPressed' else 'released')+';'
        assert body.count(anchor)==1
        value='pressed' if flag=='workbenchPressed' else 'released'
        body=body.replace(anchor, f'    return {value} || CORE.Input.Mouse.{flag}[button];')
        core=core[:start]+body+core[end:]
    callback='    CORE.Input.Mouse.currentButtonState[button] = action;'
    assert glfw.count(callback)==1
    glfw=glfw.replace(callback, callback+'\n    if (action == GLFW_PRESS) CORE.Input.Mouse.workbenchPressed[button] = true;\n    if (action == GLFW_RELEASE) CORE.Input.Mouse.workbenchReleased[button] = true;')
    reset='    for (int i = 0; i < MAX_MOUSE_BUTTONS; i++) CORE.Input.Mouse.previousButtonState[i] = CORE.Input.Mouse.currentButtonState[i];'
    assert glfw.count(reset)==1
    glfw=glfw.replace(reset, '    for (int i = 0; i < MAX_MOUSE_BUTTONS; i++) {\n        CORE.Input.Mouse.previousButtonState[i] = CORE.Input.Mouse.currentButtonState[i];\n        CORE.Input.Mouse.workbenchPressed[i] = false;\n        CORE.Input.Mouse.workbenchReleased[i] = false;\n    }')
    return MARKER+core, MARKER+glfw


def main():
    source=Path(sys.argv[1])/'src'
    paths=[source/'rcore.c',source/'platforms/rcore_desktop_glfw.c']
    texts=[path.read_text() for path in paths]
    if all(text.startswith(MARKER) for text in texts): return
    if any(text.startswith(MARKER) for text in texts):
        raise RuntimeError('Partially patched Raylib input; recreate the viewer build directory')
    modified=adapt(*texts)
    for path,text in zip(paths,modified): path.write_text(text)


if __name__=='__main__': main()
