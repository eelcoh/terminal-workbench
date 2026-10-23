#!/usr/bin/env python3
"""Build the runtime and its VT library with a pinned Zig compiler and a child-only environment."""
from pathlib import Path
import os
import platform
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
    if sys.argv[1:]:
        print("usage: build-runtime.py",file=sys.stderr); return 2
    try:
        binary, environment = build_environment(ROOT, os.environ)
        print(f"Building with Zig {ZIG_VERSION}: {binary}", flush=True)
        for command in ([sys.executable,str(ROOT / "scripts/prepare-agent-launchers.py")],
                        [sys.executable,str(ROOT / "scripts/prepare-runtime-vt.py")],
                        ["cargo","build","--locked","--bin","workbench-session"]):
            subprocess.run(command, cwd=ROOT, env=environment, check=True)
    except subprocess.CalledProcessError as error:
        return error.returncode if error.returncode > 0 else 1
    except (OSError, RuntimeError) as error:
        print(f"Build failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
