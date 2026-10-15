#!/usr/bin/env python3
"""Build the viewer with a pinned Zig compiler and a child-only environment."""
from pathlib import Path
import os
import platform
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
ZIG_VERSION = "0.16.0"


def zig_version(binary):
    try:
        result = subprocess.run([str(binary), "version"], capture_output=True,
                                text=True, check=True, timeout=10)
    except (OSError, subprocess.SubprocessError):
        return None
    return result.stdout.strip()


def find_zig(root, environment):
    override = environment.get("ZIG")
    if override:
        binary = Path(override).expanduser()
        if not binary.is_absolute() and binary.parent == Path("."):
            found = shutil.which(override, path=environment.get("PATH", ""))
            binary = Path(found) if found else binary
        binary = binary.resolve()
        version = zig_version(binary)
        if version != ZIG_VERSION:
            raise RuntimeError(f"ZIG={binary} must run Zig {ZIG_VERSION}; got {version or 'an unavailable executable'}")
        return binary

    system = {"Darwin": "macos", "Linux": "linux"}.get(platform.system())
    architecture = {"arm64": "aarch64", "aarch64": "aarch64", "x86_64": "x86_64", "AMD64": "x86_64"}.get(platform.machine())
    candidates = []
    if system and architecture:
        candidates.append(root / ".local/tools" / f"zig-{architecture}-{system}-{ZIG_VERSION}" / "zig")
    candidates.append(root / ".local/tools/zig/zig")
    found = shutil.which("zig", path=environment.get("PATH", ""))
    if found:
        candidates.append(Path(found))
    for binary in candidates:
        if zig_version(binary) == ZIG_VERSION:
            return binary.resolve()
    raise RuntimeError(f"Cannot find Zig {ZIG_VERSION}. Extract it under .local/tools as described in docs/macos-build.md, put it on PATH, or set ZIG=/absolute/path/to/zig.")


def build_environment(root, environment):
    binary = find_zig(root, environment)
    result = dict(environment)
    result["ZIG"] = str(binary)
    result["PATH"] = str(binary.parent) + os.pathsep + environment.get("PATH", "")
    return binary, result


def main():
    runtime_only=sys.argv[1:]==["--runtime-only"]
    wayland=sys.argv[1:]==["--wayland"]
    if sys.argv[1:] and not (runtime_only or wayland):
        print("usage: build-spike.py [--runtime-only | --wayland]",file=sys.stderr); return 2
    if wayland and platform.system() != "Linux":
        print("The Wayland spike requires Linux",file=sys.stderr); return 2
    try:
        binary, environment = build_environment(ROOT, os.environ)
        print(f"Building with Zig {ZIG_VERSION}: {binary}", flush=True)
        source, build = ROOT / ".local/ghostling-viewer", ROOT / (".local/build/viewer-wayland" if wayland else ".local/build/viewer")
        if runtime_only:
            subprocess.run([sys.executable,str(ROOT / "scripts/prepare-runtime-vt.py")],cwd=ROOT,env=environment,check=True)
            subprocess.run(["cargo","build","--locked","--bin","workbench-session"],cwd=ROOT,env=environment,check=True)
            return 0
        commands = [
            [sys.executable, str(ROOT / "scripts/prepare-agent-launchers.py")],
            [sys.executable, str(ROOT / "scripts/prepare-ghostling.py")],
            ["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
             f"-DGHOSTLING_ZIG_EXECUTABLE:FILEPATH={binary}", f"-DZIG_EXECUTABLE:FILEPATH={binary}",
             *(["-DGLFW_BUILD_WAYLAND=ON", "-DGLFW_BUILD_X11=OFF"] if wayland else [])],
            ["cmake", "--build", str(build), "--parallel", "2"],
            [sys.executable,str(ROOT / "scripts/prepare-runtime-vt.py")],
            ["cargo", "build", "--locked", "--bin", "workbench-session"],
        ]
        for command in commands:
            print("+ " + shlex.join(command), flush=True)
            subprocess.run(command, cwd=ROOT, env=environment, check=True)
    except subprocess.CalledProcessError as error:
        return error.returncode if error.returncode > 0 else 1
    except (OSError, RuntimeError) as error:
        print(f"Build failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
