#!/usr/bin/env python3
"""Verify independent terminal state and input on an isolated display."""
from chrome_gui_actions import action as chrome_action, workspace as chrome_workspace
from gui_smoke import close_window
from pathlib import Path
import os
import select
import signal
import shutil
import subprocess
import sys
import tempfile
import time

from session_smoke import wait_until, attach

def main():
    server_binary, viewer_binary = [str(Path(arg).resolve()) for arg in sys.argv[1:3]]
    with tempfile.TemporaryDirectory(prefix="workbench-workspace-test-") as directory:
        root = Path(directory)
        processes = []
        with (root / "test.log").open("w+") as log:
            try:
                read_fd, write_fd = os.pipe()
                xvfb = subprocess.Popen(["Xvfb", "-displayfd", str(write_fd), "-screen", "0", "1440x1000x24", "-nolisten", "tcp"], pass_fds=(write_fd,), stdout=log, stderr=log)
                processes.append(xvfb)
                os.close(write_fd)
                assert select.select([read_fd], [], [], 5)[0]
                display = os.read(read_fd, 32).decode().strip()
                os.close(read_fd)
                catalog = root / "workspaces.tsv"
                sockets = [root / f"pane-{i}.sock" for i in range(4)]
                catalog.write_text(f"Main\t{sockets[0]}\t{sockets[1]}\nScratch\t{sockets[2]}\t{sockets[3]}\n")
                for socket in sockets:
                    server = subprocess.Popen([server_binary, str(socket), "/bin/sh"], cwd=root, stdout=log, stderr=log)
                    processes.append(server)
                    wait_until(socket.exists)
                environment = dict(os.environ, DISPLAY=f":{display}", WORKBENCH_WORKSPACES=str(catalog), LIBGL_ALWAYS_SOFTWARE="1")
                environment.pop("WAYLAND_DISPLAY", None)
                environment.pop("WORKBENCH_SOCKET", None)
                def command(*args):
                    result = subprocess.run(["xdotool", *map(str, args)], env=environment, capture_output=True, text=True, timeout=5)
                    result.check_returncode()
                    return result.stdout.strip()
                def launch():
                    viewer = subprocess.Popen([viewer_binary], env=environment, cwd=root, stdout=log, stderr=log)
                    processes.append(viewer)
                    def find():
                        assert viewer.poll() is None, f"viewer exited: {viewer.returncode}"
                        result = subprocess.run(["xdotool", "search", "--onlyvisible", "--pid", str(viewer.pid)], env=environment, capture_output=True, text=True, timeout=5)
                        return result.stdout.strip().splitlines()[-1] if result.returncode == 0 else None
                    window = wait_until(find, timeout=10)
                    command("windowfocus", window)
                    time.sleep(0.5)
                    return viewer, window
                def key(name):
                    command("keydown", "--window", window, name)
                    time.sleep(0.06)
                    command("keyup", "--window", window, name)
                    time.sleep(0.06)
                def type_line(text):
                    command("type", "--clearmodifiers", "--window", window, "--delay", 15, text)
                    key("Return")
                def click(x, y):
                    command("mousemove", "--window", window, x, y)
                    command("mousedown", 1)
                    time.sleep(0.06)
                    command("mouseup", 1)
                    time.sleep(0.08)
                def read(name):
                    return wait_until(lambda: (root / name).exists() and (root / name).read_text())
                def select_word(left):
                    command("mousemove", "--window", window, left + 2, 94)
                    command("mousedown", 1)
                    time.sleep(0.06)
                    command("mousemove", "--window", window, left + 8 * 7 + 5, 94)
                    time.sleep(0.06)
                    command("mouseup", 1)
                    time.sleep(0.1)
                def clipboard():
                    return subprocess.check_output(["xclip", "-selection", "clipboard", "-o"], env=environment, timeout=5).decode()
                viewer, window = launch()
                for index, (workspace_y, pane_x) in enumerate([(106, 300), (106, 800), (150, 300), (150, 800)]):
                    chrome_workspace(click,(workspace_y-106)//44)
                    click(pane_x, 100)
                    type_line(f"keep=pane{index}; printf '%s:%s' \"$$\" \"$keep\" > pid-{index}; stty size > size-{index}")
                    assert read(f"pid-{index}").endswith(f":pane{index}")
                # Queue both edges while the viewer is stopped: no frame can
                # observe an intermediate held state. This models trackpad taps.
                def tap(x, y):
                    command("mousemove", "--window", window, x, y)
                    time.sleep(0.08)
                    os.kill(viewer.pid, signal.SIGSTOP)
                    try:
                        command("mousedown", 1, "mouseup", 1)
                    finally:
                        os.kill(viewer.pid, signal.SIGCONT)
                    time.sleep(0.15)
                for index, (workspace_y, pane_x) in enumerate([(106, 300), (106, 800), (150, 300), (150, 800)]):
                    tap(22,66);tap(100,117+((workspace_y-106)//44)*52)
                    tap(pane_x, 100)
                    type_line(f"printf '%s' \"$keep\" > tapped-{index}")
                    assert read(f"tapped-{index}") == f"pane{index}"
                identities = [read(f"pid-{i}") for i in range(4)]
                assert len({value.split(':')[0] for value in identities}) == 4
                assert read("size-0") == read("size-1"), [read("size-0"), read("size-1")]
                # Both panes have independent selections; header Copy follows focus.
                chrome_workspace(click,0)
                click(300, 100)
                type_line("printf '\\033[2J\\033[Hleft-copy\\r\\n'; touch left-ready")
                wait_until(lambda: (root / "left-ready").exists())
                time.sleep(0.1)
                select_word(58)
                key("ctrl+shift+c")
                assert clipboard() == "left-copy", repr(clipboard())
                click(800, 100)
                type_line("printf '\\033[2J\\033[Hrightcopy\\r\\n'; touch right-ready")
                wait_until(lambda: (root / "right-ready").exists())
                time.sleep(0.1)
                select_word(651)
                chrome_action(command,window,'Copy selection')
                assert clipboard() == "rightcopy", repr(clipboard())
                # Refocus by title, preserving the original pane's selection.
                click(300, 58)
                chrome_action(command,window,'Copy selection')
                assert clipboard() == "left-copy", repr(clipboard())
                # Paste must use the focused pane's transport, not its sibling's.
                click(800, 58)
                owner = subprocess.Popen(["xclip", "-selection", "clipboard", "-quiet"], stdin=subprocess.PIPE, stdout=log, stderr=log, env=environment)
                processes.append(owner)
                owner.stdin.write("printf '%s' \"$keep:café\" > paste-pane".encode())
                owner.stdin.close()
                time.sleep(0.1)
                key("ctrl+shift+v")
                key("Return")
                assert read("paste-pane") == "pane1:café"
                # Divider resize must update both PTYs without sharing dimensions.
                command("mousemove", "--window", window, 642, 400)
                command("mousedown", 1)
                time.sleep(0.06)
                command("mousemove", "--window", window, 450, 400)
                time.sleep(0.12)
                command("mouseup", 1)
                time.sleep(0.08)  # Let the frame-based viewer observe release before refocusing.
                click(300, 58)
                type_line("stty size > resized-left")
                click(900, 58)
                type_line("stty size > resized-right")
                left_rows, left_cols = map(int, read("resized-left").split())
                right_rows, right_cols = map(int, read("resized-right").split())
                assert left_rows == right_rows and left_cols < right_cols, (left_cols, right_cols)
                # A hidden session continues receiving output and answering VT queries.
                type_line("sleep 0.6; printf background-finished; touch background-done")
                chrome_workspace(click,1)
                wait_until(lambda: (root / "background-done").exists())
                click(300, 58)
                type_line("vi editor.txt")
                time.sleep(0.5)
                command("type", "--window", window, "--delay", 20, "iworkspace editor")
                key("Escape")
                type_line(":wq")
                assert read("editor.txt") == "workspace editor\n"
                # Visible Close detaches every session. Reopen preserves all PIDs and variables.
                close_window(window,environment)
                assert viewer.wait(timeout=5) == 0
                for value in identities: os.kill(int(value.split(':')[0]), 0)
                viewer, window = launch()
                for index, (workspace_y, pane_x) in enumerate([(106, 300), (106, 800), (150, 300), (150, 800)]):
                    chrome_workspace(click,(workspace_y-106)//44)
                    click(pane_x, 58)
                    type_line(f"printf '%s:%s' \"$$\" \"$keep\" > reattached-{index}")
                    assert read(f"reattached-{index}") == identities[index]
                # Shell exit affects only that pane; sibling input remains functional.
                type_line("exit")
                time.sleep(0.2)
                click(300, 58)
                type_line("printf alive > sibling-alive")
                assert read("sibling-alive") == "alive"
                key("F12")
                screenshot = root / "screenshot000.png"
                wait_until(screenshot.exists)
                artifacts = Path(__file__).resolve().parent.parent / ".local/test-results"
                artifacts.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(screenshot, artifacts / "workspaces.png")
                close_window(window, environment)
                assert viewer.wait(timeout=5) == 0
                print("PASS: same-poll taps switch workspaces/panes and release selection, four independent shells, workspace switching, focused input/clipboard, per-pane selection, divider resize, hidden output, vi, same-PID reconnect, isolated shell exit")
            finally:
                if sys.exc_info()[0] and "viewer" in locals() and viewer.poll() is None:
                    key("F12")
                    screenshot = root / "screenshot000.png"
                    if screenshot.exists():
                        artifacts = Path(__file__).resolve().parent.parent / ".local/test-results"
                        artifacts.mkdir(parents=True, exist_ok=True)
                        shutil.copyfile(screenshot, artifacts / "workspace-failure.png")
                    viewer.terminate()
                    viewer.wait(timeout=5)
                    time.sleep(0.1)
                    for socket in sockets:
                        if socket.exists():
                            connection, replay, _ = attach(socket)
                            print(f"{socket.name}: {bytes(replay[-1500:])!r}", file=sys.stderr)
                            connection.close()
                for process in reversed(processes):
                    if process.poll() is None:
                        process.terminate()
                        process.wait(timeout=5)
                if sys.exc_info()[0]:
                    log.seek(0)
                    print(log.read(), file=sys.stderr)

if __name__ == "__main__":
    main()
