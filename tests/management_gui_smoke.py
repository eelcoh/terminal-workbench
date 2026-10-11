#!/usr/bin/env python3
"""Create/rename through the GUI and verify persistent live shell identities."""
from pathlib import Path
import json
import os
import select
import shutil
import subprocess
import sys
import tempfile
import time
import uuid
from gui_smoke import close_window
from session_smoke import wait_until, attach, send

ROOT = Path(__file__).resolve().parent.parent

def main():
    runtime, viewer_binary = [str(Path(arg).resolve()) for arg in sys.argv[1:3]]
    helper = ROOT / "scripts/workspace_catalog.py"
    with tempfile.TemporaryDirectory(prefix="wb-manage-") as directory:
        root = Path(directory)
        state = root / "sessions"
        state.mkdir()
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
                environment = dict(os.environ, DISPLAY=f":{display}", WORKBENCH_WORKSPACES=str(state / "workspaces.tsv"),
                    WORKBENCH_CATALOG_SCRIPT=str(helper), WORKBENCH_STATE_DIR=str(state),
                    WORKBENCH_RUNTIME=runtime, WORKBENCH_PROJECT=str(root), LIBGL_ALWAYS_SOFTWARE="1", SHELL="/bin/sh")
                environment.pop("WAYLAND_DISPLAY", None)
                environment.pop("WORKBENCH_SOCKET", None)
                bootstrap = ["python3", str(helper), "--state-dir", str(state), "--runtime", runtime, "--cwd", str(root)]
                subprocess.run(bootstrap, env=environment, check=True, stdout=log, stderr=log, timeout=10)
                def command(*args):
                    result = subprocess.run(["xdotool", *map(str, args)], env=environment, capture_output=True, text=True, timeout=5)
                    result.check_returncode()
                    return result.stdout.strip()
                def launch():
                    viewer = subprocess.Popen([viewer_binary], env=environment, cwd=root, stdout=log, stderr=log)
                    processes.append(viewer)
                    def find():
                        assert viewer.poll() is None, viewer.returncode
                        result = subprocess.run(["xdotool", "search", "--onlyvisible", "--pid", str(viewer.pid)], env=environment, capture_output=True, text=True)
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
                def click(x, y):
                    command("mousemove", "--window", window, x, y)
                    command("mousedown", 1)
                    time.sleep(0.06)
                    command("mouseup", 1)
                    time.sleep(0.08)
                def type_text(text):
                    command("type", "--clearmodifiers", "--window", window, "--delay", 25, "--", text)
                def line(text):
                    type_text(text)
                    key("Return")
                def read(name):
                    return wait_until(lambda: (root / name).exists() and (root / name).read_text())
                def metadata():
                    return json.loads((state / "workspaces.json").read_text())
                def edit(x, y, title, predicate):
                    click(x, y)
                    type_text(title)
                    key("Return")
                    wait_until(lambda: predicate(metadata()), timeout=10)
                    time.sleep(0.3)
                viewer, window = launch()
                line("keep=retained; printf '%s' \"$$\" > original.pid")
                original_pid = read("original.pid")
                edit(84, 641, "Projects", lambda data: data["workspaces"][0]["name"] == "Projects")
                edit(667, 58, "-Build", lambda data: data["workspaces"][0]["terminals"][0]["name"] == "-Build")
                line("printf '%s:%s' \"$$\" \"$keep\" > renamed.pid")
                assert read("renamed.pid") == f"{original_pid}:retained"
                # Escape and Cancel must not rename or feed their text into a shell.
                before = (state / "workspaces.json").read_bytes()
                click(667, 58)
                type_text("cancelled")
                key("Escape")
                assert (state / "workspaces.json").read_bytes() == before
                line("printf cancelled-safely > cancelled-result")
                assert read("cancelled-result") == "cancelled-safely"
                # Empty names report an error and keep the editor open.
                click(84, 641)
                key("BackSpace")
                key("Return")
                time.sleep(0.3)
                assert (state / "workspaces.json").read_bytes() == before
                owner = subprocess.Popen(["xclip", "-selection", "clipboard", "-quiet"], stdin=subprocess.PIPE, stdout=log, stderr=log, env=environment)
                processes.append(owner)
                owner.stdin.write(b"Projects")
                owner.stdin.close()
                time.sleep(0.1)
                key("ctrl+v")
                key("Return")
                time.sleep(0.4)
                edit(84, 603, "Research", lambda data: len(data["workspaces"]) == 3)
                line("keep=research; printf '%s' \"$$\" > research.pid")
                research_pid = read("research.pid")
                assert research_pid != original_pid
                edit(84, 679, "Tests", lambda data: len(data["workspaces"][2]["terminals"]) == 2)
                line("keep=tests; printf '%s' \"$$\" > tests.pid")
                tests_pid = read("tests.pid")
                edit(84, 679, "Checks", lambda data: len(data["workspaces"][2]["terminals"]) == 3)
                line("keep=checks; printf '%s' \"$$\" > checks.pid")
                checks_pid = read("checks.pid")
                assert len({original_pid, research_pid, tests_pid, checks_pid}) == 4
                # Previous/Next reveals offscreen terminals and keeps their process state.
                click(223, 16)
                line("printf '%s:%s' \"$$\" \"$keep\" > tests-again")
                assert read("tests-again") == f"{tests_pid}:tests"
                click(223, 16)
                line("printf '%s:%s' \"$$\" \"$keep\" > research-again")
                assert read("research-again") == f"{research_pid}:research"
                click(287, 16)
                click(287, 16)
                edit(1190, 58, "Verification", lambda data: data["workspaces"][2]["terminals"][2]["name"] == "Verification")
                # Raylib also forwards F12 as terminal input. Capture with the
                # name editor open so the modal consumes it without shell input.
                click(1190, 58)
                key("F12")
                screenshot = root / "screenshot000.png"
                wait_until(screenshot.exists)
                artifacts = ROOT / ".local/test-results"
                artifacts.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(screenshot, artifacts / "management.png")
                key("Escape")
                click(1196, 16)
                assert viewer.wait(timeout=5) == 0
                for value in [original_pid, research_pid, tests_pid, checks_pid]: os.kill(int(value), 0)
                subprocess.run(bootstrap, env=environment, check=True, stdout=log, stderr=log, timeout=10)
                assert metadata()["workspaces"][0]["name"] == "Projects"
                assert metadata()["workspaces"][0]["terminals"][0]["name"] == "-Build"
                assert metadata()["workspaces"][2]["terminals"][2]["name"] == "Verification"
                viewer, window = launch()
                line("printf '%s:%s' \"$$\" \"$keep\" > restored-original")
                assert read("restored-original") == f"{original_pid}:retained"
                click(60, 194)
                line("printf '%s:%s' \"$$\" \"$keep\" > restored-research")
                assert read("restored-research") == f"{research_pid}:research"
                click(287, 16)
                click(287, 16)
                line("printf '%s:%s' \"$$\" \"$keep\" > restored-checks")
                assert read("restored-checks") == f"{checks_pid}:checks"
                # Keep the existing list-specific navigation/geometry coverage.
                click(522, 16)
                click(872, 74)  # List
                # Overview filtering and cancellation cannot feed text to the shell.
                type_text("no-matching-terminal")
                key("Return")
                key("Escape")
                line("printf overview-cancel-safe > overview-cancelled")
                assert read("overview-cancelled") == "overview-cancel-safe"
                key("ctrl+shift+o")
                type_text("-build")
                key("Return")
                line("printf '%s:%s' \"$$\" \"$keep\" > overview-original")
                assert read("overview-original") == f"{original_pid}:retained"
                key("ctrl+shift+o")
                type_text("RESEARCH")
                key("Down")
                key("Down")
                key("Return")
                line("printf '%s:%s' \"$$\" \"$keep\" > overview-third")
                assert read("overview-third") == f"{checks_pid}:checks"
                line("sleep 0.4; printf overview-background; touch overview-background-done")
                click(522, 16)
                type_text("Research")
                wait_until(lambda: (root / "overview-background-done").exists())
                key("F12")
                time.sleep(0.2)
                screenshots = sorted(root.glob("screenshot*.png"))
                shutil.copyfile(screenshots[-1], artifacts / "overview.png")
                click(320, 183)
                line("printf '%s:%s' \"$$\" \"$keep\" > overview-click")
                assert read("overview-click") == f"{research_pid}:research"
                # At minimum geometry one row is visible: wheel/keys reveal the rest.
                command("windowsize", window, 800, 300)
                time.sleep(0.3)
                key("ctrl+shift+o")
                type_text("Research")
                command("mousemove", "--window", window, 300, 180)
                command("click", "--repeat", 2, "--delay", 80, 5)
                time.sleep(0.1)
                click(300, 183)
                line("printf '%s:%s' \"$$\" \"$keep\" > overview-scrolled")
                assert read("overview-scrolled") == f"{checks_pid}:checks"
                key("ctrl+shift+o")
                type_text("Research")
                key("Down")
                key("Down")
                key("Return")
                command("windowsize", window, 1240, 760)
                time.sleep(0.3)
                line("printf '%s:%s' \"$$\" \"$keep\" > overview-keyboard-small")
                assert read("overview-keyboard-small") == f"{checks_pid}:checks"
                # Runtime identity appears in the overview and its Agents filter.
                # This named sleep fixture proves the adapter, not a real CLI's UI.
                fixture = root / "codex"
                shutil.copyfile("/usr/bin/sleep", fixture)
                fixture.chmod(0o700)
                checks_id = metadata()["workspaces"][2]["terminals"][2]["id"]
                checks_socket = state / f"{checks_id}.sock"
                def agent_identity():
                    result = subprocess.run([runtime, "inspect", str(checks_socket)], capture_output=True, text=True, check=True, timeout=5)
                    return json.loads(result.stdout)
                line(f"sh -c 'echo $$ > attention.pid; exec {fixture} 120'")
                wait_until(lambda: agent_identity()["agent"] == "codex")
                assert agent_identity()["status"] == "unknown"
                hook_environment = dict(os.environ, WORKBENCH_SESSION_SOCKET=str(checks_socket),
                    WORKBENCH_AGENT_GROUP=str(os.getpgid(int(read("attention.pid")))), WORKBENCH_AGENT_RUN=uuid.uuid4().hex)
                def report(event, status):
                    result = subprocess.run(["python3", str(ROOT / "scripts/agent-hook.py"), "codex"],
                        input=json.dumps({"hook_event_name": event}), text=True, capture_output=True, env=hook_environment, timeout=3)
                    assert result.returncode == 0 and result.stdout == result.stderr == ""
                    wait_until(lambda: agent_identity()["status"] == status)
                    time.sleep(0.15)
                report("UserPromptSubmit", "working")
                time.sleep(0.2)
                click(522, 16)
                type_text("codex")
                click(844, 74)
                key("F12")
                time.sleep(0.2)
                screenshots = sorted(root.glob("screenshot*.png"))
                shutil.copyfile(screenshots[-1], artifacts / "agents.png")
                click(740, 74)  # Needs input filter excludes Working.
                key("Return")  # Empty results keep the overview open.
                report("PermissionRequest", "waiting")
                key("F12")
                time.sleep(0.2)
                screenshots = sorted(root.glob("screenshot*.png"))
                shutil.copyfile(screenshots[-1], artifacts / "attention.png")
                key("Return")
                report("Stop", "ready")
                key("ctrl+c")
                wait_until(lambda: agent_identity()["agent"] is None)
                assert agent_identity()["status"] == "unknown"
                line("printf '%s:%s' \"$$\" \"$keep\" > after-agent")
                assert read("after-agent") == f"{checks_pid}:checks"
                # Identity disappearing empties the Agents filter without closing it.
                click(522, 16)
                key("Return")
                click(844, 74)
                click(740, 74)
                key("Escape")
                # Stop the middle terminal while a foreground job is running.
                click(223, 16)
                line("python3 -c 'import os,time;open(\"job.pid\",\"w\").write(str(os.getpid()));time.sleep(120)'")
                job_pid = read("job.pid")
                before = (state / "workspaces.json").read_bytes()
                click(602, 58)
                key("Return")  # Destructive confirmation requires an explicit click.
                time.sleep(0.2)
                assert (state / "workspaces.json").read_bytes() == before
                key("Escape")
                os.kill(int(job_pid), 0)
                click(602, 58)
                key("F12")
                time.sleep(0.2)
                screenshots = sorted(root.glob("screenshot*.png"))
                shutil.copyfile(screenshots[-1], artifacts / "close-confirmation.png")
                click(761, 452)
                wait_until(lambda: len(metadata()["workspaces"][2]["terminals"]) == 2, timeout=10)
                def stopped(value):
                    try:
                        return Path(f"/proc/{value}/stat").read_text().split(") ", 1)[1].startswith("Z")
                    except FileNotFoundError:
                        return True
                wait_until(lambda: stopped(job_pid) and stopped(tests_pid))
                time.sleep(0.4)
                # Removing a pane must preserve its surviving sibling's shell.
                click(287, 16)
                line("printf '%s:%s' \"$$\" \"$keep\" > after-middle-close")
                assert read("after-middle-close") == f"{checks_pid}:checks"
                click(163, 62)
                key("Escape")
                assert len(metadata()["workspaces"]) == 3
                click(163, 62)
                click(761, 452)
                wait_until(lambda: len(metadata()["workspaces"]) == 2, timeout=10)
                wait_until(lambda: stopped(research_pid) and stopped(checks_pid))
                time.sleep(0.4)
                click(60, 106)
                line("printf '%s:%s' \"$$\" \"$keep\" > after-workspace-close")
                assert read("after-workspace-close") == f"{original_pid}:retained"
                # An empty workspace remains usable, and no stopped shell revives.
                click(602, 58)
                click(761, 452)
                wait_until(lambda: len(metadata()["workspaces"][0]["terminals"]) == 1)
                time.sleep(0.3)
                click(1125, 58)
                click(761, 452)
                wait_until(lambda: len(metadata()["workspaces"][0]["terminals"]) == 0)
                time.sleep(0.3)
                edit(84, 679, "Fresh", lambda data: len(data["workspaces"][0]["terminals"]) == 1)
                line("printf fresh > fresh-shell")
                assert read("fresh-shell") == "fresh"
                for row in [150, 106]:
                    click(60, row)
                    click(163, 62)
                    click(761, 452)
                    expected = 1 if row == 150 else 0
                    wait_until(lambda: len(metadata()["workspaces"]) == expected)
                    time.sleep(0.3)
                assert not list(state.glob("*.sock"))
                click(522, 16)
                key("Return")
                key("Escape")
                assert metadata()["workspaces"] == []
                close_window(window, environment)
                assert viewer.wait(timeout=5) == 0
                subprocess.run(bootstrap, env=environment, check=True, stdout=log, stderr=log, timeout=10)
                assert metadata()["workspaces"] == [] and not list(state.glob("*.sock"))
                viewer, window = launch()
                edit(84, 603, "Restart", lambda data: len(data["workspaces"]) == 1)
                line("printf empty-recovery > recovery")
                assert read("recovery") == "empty-recovery"
                # Opening saved organization does not resurrect an exited shell.
                recovered_id = metadata()["workspaces"][0]["terminals"][0]["id"]
                recovered_path = state / f"{recovered_id}.sock"
                line("exit")
                wait_until(lambda: not recovered_path.exists())
                close_window(window, environment)
                assert viewer.wait(timeout=5) == 0
                subprocess.run(bootstrap, env=environment, check=True, stdout=log, stderr=log, timeout=10)
                assert not recovered_path.exists()
                viewer, window = launch()
                assert viewer.poll() is None and not recovered_path.exists()
                subprocess.run(bootstrap + ["--command", "terminal.recover", "--id", recovered_id],
                               env=environment, check=True, stdout=log, stderr=log, timeout=10)
                click(1050, 58)
                time.sleep(.6)
                line("printf explicit-recovery > recovered-shell")
                assert read("recovered-shell") == "explicit-recovery"
                close_window(window, environment)
                assert viewer.wait(timeout=5) == 0
                print("PASS: GUI create/rename, unchanged PIDs on rename, cancelled/invalid names, third-terminal navigation, persisted names and same-shell reopen; confirmed stops, foreground job termination, middle-pane removal, surviving PIDs, empty states and no resurrection; overview filtering, modal isolation, cross-workspace/third-pane jumps, hidden output, scrolling and minimum-size keyboard navigation; runtime agent labels, lifecycle status, attention filtering and clearing")
            finally:
                if "viewer" in locals() and viewer.poll() is None:
                    if sys.exc_info()[0]:
                        key("F12")
                        screenshot = root / "screenshot000.png"
                        if screenshot.exists():
                            artifacts = ROOT / ".local/test-results"
                            artifacts.mkdir(parents=True, exist_ok=True)
                            shutil.copyfile(screenshot, artifacts / "management-failure.png")
                    viewer.terminate()
                    viewer.wait(timeout=5)
                time.sleep(0.15)
                for socket in state.glob("*.sock"):
                    connection = None
                    try:
                        connection, replay, _ = attach(socket)
                        if sys.exc_info()[0]: print(f"{socket.name}: {bytes(replay[-500:])!r}", file=sys.stderr)
                        send(connection, 16, b"\x03\nexit\n")
                        connection.close()
                        connection = None
                        wait_until(lambda: not socket.exists())
                    finally:
                        if connection: connection.close()
                for process in reversed(processes):
                    if process.poll() is None:
                        process.terminate()
                        process.wait(timeout=5)
                if sys.exc_info()[0]:
                    log.seek(0)
                    print(log.read(), file=sys.stderr)

if __name__ == "__main__":
    main()
