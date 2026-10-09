#!/usr/bin/env python3
"""Exercise the real viewer in an isolated Xvfb display, never the user's desktop."""
from pathlib import Path
import ctypes
import ctypes.util
import os
import select
import signal
import shutil
import subprocess
import sys
import tempfile
import time

from session_smoke import wait_until, attach

def close_window(window, environment):
    # Xvfb has no window manager. Deliver WM_DELETE_WINDOW directly to GLFW.
    library = ctypes.CDLL(ctypes.util.find_library("X11"))
    library.XOpenDisplay.argtypes = [ctypes.c_char_p]
    library.XOpenDisplay.restype = ctypes.c_void_p
    library.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
    library.XInternAtom.restype = ctypes.c_ulong
    library.XSendEvent.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_long, ctypes.c_void_p]
    library.XFlush.argtypes = [ctypes.c_void_p]
    library.XCloseDisplay.argtypes = [ctypes.c_void_p]
    class Message(ctypes.Structure):
        _fields_ = [("type", ctypes.c_int), ("serial", ctypes.c_ulong),
                    ("send_event", ctypes.c_int), ("display", ctypes.c_void_p),
                    ("window", ctypes.c_ulong), ("message_type", ctypes.c_ulong),
                    ("format", ctypes.c_int), ("data", ctypes.c_long * 5)]
    display = library.XOpenDisplay(environment["DISPLAY"].encode())
    assert display
    message = Message()
    message.type, message.display, message.window, message.format = 33, display, int(window), 32
    message.message_type = library.XInternAtom(display, b"WM_PROTOCOLS", 0)
    message.data[0] = library.XInternAtom(display, b"WM_DELETE_WINDOW", 0)
    assert library.XSendEvent(display, int(window), 0, 0, ctypes.byref(message))
    library.XFlush(display)
    library.XCloseDisplay(display)

def main():
    server_binary, viewer_binary = [str(Path(argument).resolve()) for argument in sys.argv[1:3]]
    with tempfile.TemporaryDirectory(prefix="workbench-gui-test-") as directory:
        root = Path(directory)
        socket_path = root / "session.sock"
        read_fd, write_fd = os.pipe()
        processes = []
        with (root / "smoke.log").open("w+") as log:
            try:
                xvfb = subprocess.Popen(["Xvfb", "-displayfd", str(write_fd), "-screen", "0", "1280x900x24", "-nolisten", "tcp"], pass_fds=(write_fd,), stdout=log, stderr=log)
                processes.append(xvfb)
                os.close(write_fd)
                assert select.select([read_fd], [], [], 5)[0], "Xvfb did not provide a display"
                display_number = os.read(read_fd, 32).decode().strip()
                os.close(read_fd)
                environment = dict(os.environ, DISPLAY=f":{display_number}", WORKBENCH_SOCKET=str(socket_path), LIBGL_ALWAYS_SOFTWARE="1")
                environment.pop("WAYLAND_DISPLAY", None)
                environment.pop("WORKBENCH_WORKSPACES", None)
                server = subprocess.Popen([server_binary, str(socket_path), "/bin/sh"], cwd=root, stdout=log, stderr=log)
                processes.append(server)
                wait_until(socket_path.exists)
                def command(*arguments):
                    return subprocess.run(["xdotool", *arguments], env=environment, capture_output=True, text=True, timeout=5)
                def launch():
                    viewer = subprocess.Popen([viewer_binary], env=environment, cwd=root, stdout=log, stderr=log)
                    processes.append(viewer)
                    def find():
                        if viewer.poll() is not None:
                            raise AssertionError(f"viewer exited with {viewer.returncode}")
                        result = command("search", "--onlyvisible", "--pid", str(viewer.pid))
                        return result.stdout.strip().splitlines()[-1] if result.returncode == 0 else None
                    window = wait_until(find, timeout=10)
                    command("windowfocus", window).check_returncode()
                    time.sleep(0.4)
                    return viewer, window
                def type_line(window, text):
                    command("type", "--clearmodifiers", "--window", window, "--delay", "25", text).check_returncode()
                    command("keydown", "--window", window, "Return").check_returncode()
                    time.sleep(0.05)
                    command("keyup", "--window", window, "Return").check_returncode()
                def key(window, name):
                    command("keydown", "--window", window, name).check_returncode()
                    time.sleep(0.05)
                    command("keyup", "--window", window, name).check_returncode()
                viewer, window = launch()
                type_line(window, "printf '%s' \"$$\" > first.pid; keep=retained")
                wait_until(lambda: (root / "first.pid").exists() and (root / "first.pid").read_text())
                pid = (root / "first.pid").read_text()
                command("windowsize", window, "1080", "720").check_returncode()
                time.sleep(0.2)
                type_line(window, "stty size > dimensions")
                dimensions = wait_until(lambda: (root / "dimensions").exists() and (root / "dimensions").read_text())
                rows, cols = map(int, dimensions.split())
                assert rows > 24 and cols > 80, dimensions
                def mouse_click(x, y):
                    command("mousemove", "--window", window, str(x), str(y)).check_returncode()
                    command("mousedown", "1").check_returncode()
                    time.sleep(0.06)
                    command("mouseup", "1").check_returncode()
                    time.sleep(0.06)
                def select_text(start, end, row=0, end_row=None):
                    command("mousemove", "--window", window, str(4 + start * 7 + (5 if start > end else 2)), str(32 + 4 + row * 16 + 8)).check_returncode()
                    command("mousedown", "1").check_returncode()
                    time.sleep(0.06)
                    command("mousemove", "--window", window, str(4 + end * 7 + (2 if start > end else 5)), str(32 + 4 + (row if end_row is None else end_row) * 16 + 8)).check_returncode()
                    time.sleep(0.06)
                    command("mouseup", "1").check_returncode()
                    time.sleep(0.08)
                def clipboard_text():
                    return subprocess.check_output(["xclip", "-selection", "clipboard", "-o"], env=environment, timeout=5).decode()
                def own_clipboard(value):
                    owner = subprocess.Popen(["xclip", "-selection", "clipboard", "-quiet"], stdin=subprocess.PIPE, stdout=log, stderr=log, env=environment)
                    processes.append(owner)
                    owner.stdin.write(value.encode())
                    owner.stdin.close()
                    time.sleep(0.1)
                type_line(window, "printf '\\033[2J\\033[Hclipboard sample\\r\\n'; touch selection-ready")
                wait_until(lambda: (root / "selection-ready").exists())
                time.sleep(0.15)
                select_text(0, 8)
                key(window, "ctrl+shift+c")
                assert clipboard_text() == "clipboard", repr(clipboard_text())
                select_text(8, 0)
                mouse_click(888, 16)
                assert clipboard_text() == "clipboard", repr(clipboard_text())
                # Word selection uses the VT engine's repeat-click semantics.
                mouse_click(4 + 3 * 7, 44)
                mouse_click(4 + 3 * 7, 44)
                key(window, "ctrl+shift+c")
                assert clipboard_text() == "clipboard", repr(clipboard_text())
                # A soft wrap must copy as one logical line, including Unicode.
                logical_line = "W" * cols + "café"
                (root / "wrapped.txt").write_text(logical_line)
                type_line(window, "printf '\\033[2J\\033[H'; cat wrapped.txt; printf '\\r\\n'; touch wrapped-ready")
                wait_until(lambda: (root / "wrapped-ready").exists())
                time.sleep(0.15)
                select_text(0, 3, end_row=1)
                key(window, "ctrl+shift+c")
                assert clipboard_text() == logical_line, repr(clipboard_text())
                # Mouse-reporting apps keep ordinary clicks; Shift forces local selection.
                (root / "capture_mouse.py").write_text("import os, termios, tty\nold = termios.tcgetattr(0)\ntry:\n tty.setraw(0)\n os.write(1, b'\\x1b[2J\\x1b[Hmouseword\\r\\n\\x1b[?1000h\\x1b[?1006h')\n open('mouse-ready', 'w').close()\n data = b''\n while not data.endswith(b'q'):\n  data += os.read(0, 4096)\n open('mouse-bytes', 'wb').write(data)\nfinally:\n os.write(1, b'\\x1b[?1000l\\x1b[?1006l')\n termios.tcsetattr(0, termios.TCSANOW, old)\n")
                type_line(window, "python3 capture_mouse.py")
                wait_until(lambda: (root / "mouse-ready").exists())
                time.sleep(0.15)
                mouse_click(400, 200)
                command("keydown", "--window", window, "Shift_L").check_returncode()
                time.sleep(0.06)
                select_text(0, 8)
                command("keyup", "--window", window, "Shift_L").check_returncode()
                time.sleep(0.06)
                mouse_click(888, 16)
                assert clipboard_text() == "mouseword", repr(clipboard_text())
                key(window, "q")
                mouse_bytes = wait_until(lambda: (root / "mouse-bytes").exists() and (root / "mouse-bytes").read_bytes())
                assert mouse_bytes.count(b"\x1b[<") == 2 and mouse_bytes.endswith(b"q"), repr(mouse_bytes)
                own_clipboard("printf '%s' 'café' > pasted.txt")
                key(window, "ctrl+shift+v")
                key(window, "Return")
                assert wait_until(lambda: (root / "pasted.txt").exists() and (root / "pasted.txt").read_text()) == "café"
                # Capture raw bytes from a program requesting bracketed paste.
                (root / "capture_paste.py").write_text("import os, termios, tty\nold = termios.tcgetattr(0)\ntry:\n tty.setraw(0)\n os.write(1, b'\\x1b[?2004h')\n open('paste-ready', 'w').close()\n data = b''\n while not data.endswith(b'\\x1b[201~'):\n  data += os.read(0, 4096)\n open('paste-bytes', 'wb').write(data)\nfinally:\n os.write(1, b'\\x1b[?2004l')\n termios.tcsetattr(0, termios.TCSANOW, old)\n")
                type_line(window, "python3 capture_paste.py")
                wait_until(lambda: (root / "paste-ready").exists())
                own_clipboard("one\ntwo café")
                mouse_click(960, 16)
                pasted = wait_until(lambda: (root / "paste-bytes").exists() and (root / "paste-bytes").read_bytes())
                assert pasted == b"\x1b[200~one\ntwo caf\xc3\xa9\x1b[201~", repr(pasted)
                # Scroll older output, then prove copy reads the visible viewport.
                type_line(window, "printf '\\033[3J\\033[2J\\033[H'; seq -f 'line%04g' 1 100; touch scroll-ready")
                wait_until(lambda: (root / "scroll-ready").exists())
                time.sleep(0.15)
                select_text(0, 7)
                key(window, "ctrl+shift+c")
                before_scroll = clipboard_text()
                command("mousemove", "--window", window, "400", "200").check_returncode()
                for _ in range(6):
                    command("click", "4").check_returncode()
                    time.sleep(0.06)
                time.sleep(0.1)
                select_text(0, 7)
                key(window, "ctrl+shift+c")
                after_scroll = clipboard_text()
                assert before_scroll.startswith("line") and after_scroll.startswith("line"), (before_scroll, after_scroll)
                assert int(after_scroll[4:]) < int(before_scroll[4:]), (before_scroll, after_scroll)
                # Typed input returns to live output, avoiding typing into old history.
                type_line(window, "printf '\\033[2J\\033[Hlive-output\\r\\n'; touch live-ready")
                wait_until(lambda: (root / "live-ready").exists())
                time.sleep(0.15)
                select_text(0, 10)
                key(window, "ctrl+shift+c")
                assert clipboard_text() == "live-output", repr(clipboard_text())
                # Exercise an actual alternate-screen editor and its VT queries.
                type_line(window, "vi editor.txt")
                time.sleep(0.5)
                command("type", "--window", window, "--delay", "25", "iworkbench editor smoke").check_returncode()
                key(window, "Escape")
                type_line(window, ":wq")
                assert wait_until(lambda: (root / "editor.txt").exists() and (root / "editor.txt").read_text()) == "workbench editor smoke\n"
                # Escape must reach a terminal application, rather than close the app.
                key(window, "Escape")
                assert viewer.poll() is None
                type_line(window, "printf '\\033[?1049h\\033[2JFULL_SCREEN\\n'; sleep 0.3; printf 'while_detached\\n'")
                # Close through the visible in-app control, without a window manager.
                command("mousemove", "--window", window, "1036", "16").check_returncode()
                command("mousedown", "1").check_returncode()
                time.sleep(0.08)
                command("mouseup", "1").check_returncode()
                assert viewer.wait(timeout=5) == 0
                os.kill(int(pid), 0)
                time.sleep(0.4)
                viewer, window = launch()
                type_line(window, "printf '\\033[?1049l'; printf '%s:%s' \"$$\" \"$keep\" > second.pid")
                second = wait_until(lambda: (root / "second.pid").exists() and (root / "second.pid").read_text())
                assert second == f"{pid}:retained", second
                # Ctrl-C should interrupt a foreground command, without closing the viewer.
                type_line(window, "sleep 30")
                time.sleep(0.1)
                key(window, "ctrl+c")
                type_line(window, "printf interrupted > control-result")
                assert wait_until(lambda: (root / "control-result").exists() and (root / "control-result").read_text()) == "interrupted"
                type_line(window, "exit")
                assert server.wait(timeout=5) == 0
                key(window, "F12")
                screenshot = root / "screenshot000.png"
                wait_until(screenshot.exists)
                artifacts = Path(__file__).resolve().parent.parent / ".local/test-results"
                artifacts.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(screenshot, artifacts / "window-controls.png")
                close_window(window, environment)
                assert viewer.wait(timeout=5) == 0
                print("PASS: real Ghostty/Raylib window, typed shell input, vi edit/save, Ctrl-C, Escape routing, text/word/wrapped selection, mouse-app routing, clipboard shortcuts/buttons, Unicode/bracketed paste, scrollback copy, return to live input, resize, alternate-screen output, visible Close button, native window close, same-shell reattachment")
            finally:
                if sys.exc_info()[0] and "viewer" in locals() and viewer.poll() is None:
                    command("keydown", "--window", window, "F12")
                    time.sleep(0.1)
                    command("keyup", "--window", window, "F12")
                    screenshot = root / "screenshot000.png"
                    if screenshot.exists():
                        artifacts = Path(__file__).resolve().parent.parent / ".local/test-results"
                        artifacts.mkdir(parents=True, exist_ok=True)
                        shutil.copyfile(screenshot, artifacts / "gui-failure.png")
                    viewer.terminate()
                    viewer.wait(timeout=5)
                    time.sleep(0.1)
                    if server.poll() is None:
                        connection, replay, _ = attach(socket_path)
                        print(f"Last terminal bytes on failure: {bytes(replay[-4096:])!r}", file=sys.stderr)
                        connection.close()
                for process in reversed(processes):
                    if process.poll() is None:
                        process.send_signal(signal.SIGTERM)
                        process.wait(timeout=5)
                if sys.exc_info()[0]:
                    log.seek(0)
                    print(log.read(), file=sys.stderr)

if __name__ == "__main__":
    main()
