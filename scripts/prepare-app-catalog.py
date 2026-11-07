#!/usr/bin/env python3
"""Regenerate the shipped app catalog: TUIs from awesome-tuis that Homebrew installs.

awesome-tuis (no licence) only selects programs, by repository URL; names are
the projects' own. Descriptions, package names and commands come from
Homebrew's formula index and executables list (homebrew-core, BSD-2-Clause).
The output is checked in, so Band works offline and each refresh is a diff.
"""
import argparse
from pathlib import Path
import json
import re
import subprocess
import sys
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "spikes/qt-input/apps-catalog.json"
AWESOME = "git@github.com:rothgar/awesome-tuis.git"
FORMULAE = "https://formulae.brew.sh/api/formula.json"
EXECUTABLES = "https://formulae.brew.sh/api/internal/executables.txt"
# Toolkits rather than programs.
SKIP_SECTIONS = {"Libraries"}
# Listed, but they need arguments or input to do anything as a pane.
SKIP_FORMULAE = {"blink", "chafa", "ghcup", "git-delta", "helm", "sq", "talosctl", "timg", "ttyplot", "viu", "x-cmd"}
ENTRY = re.compile(r"^\s*[-*] \[([^\]]+)\]\((https?://[^)\s]+)\)\s*(.*)$")
SECTION = re.compile(r"<h2>([^<]+)</h2>|^## (.+)$")


def repository(url):
    """A forge repository as host/owner/name, or any other page as host/path."""
    match = re.match(r"https?://(?:www\.)?(github\.com|gitlab\.com|codeberg\.org|sr\.ht|git\.sr\.ht)/([^/#?]+)/([^/#?]+)", url or "", re.I)
    if not match:
        page = re.match(r"https?://(?:www\.)?([^?#]+?)/*(?:[?#].*)?$", url or "", re.I)
        return page[1].lower() if page else None
    return "/".join((match[1].lower(), match[2].lower(), re.sub(r"\.git$", "", match[3].lower())))


def forge_name(key):
    parts = key.split("/")
    return (parts[0], parts[2]) if len(parts) == 3 and parts[0] in ("github.com", "gitlab.com", "codeberg.org") else None


def awesome(readme):
    section, result = "", []
    for line in readme.splitlines():
        heading = SECTION.search(line)
        if heading:
            section = (heading[1] or heading[2]).strip()
            continue
        entry = ENTRY.match(line)
        if entry and section and section not in SKIP_SECTIONS and section != "Table of Contents":
            key = repository(entry[2])
            if key:
                result.append((entry[1].strip(), key, section))
    return result


def platforms(formula):
    files = ((formula.get("bottle") or {}).get("stable") or {}).get("files") or {}
    if not files or "all" in files:
        return True, True
    return any(not key.endswith("_linux") for key in files), any(key.endswith("_linux") for key in files)


def command(name, executables, project):
    if name in executables:
        return name
    if len(executables) == 1:
        return executables[0]
    for candidate in (project.lower(), project.lower().replace(" ", "-")):
        if candidate in executables:
            return candidate
    return None


def generate(readme, formulae, executables, revision):
    by_repository = {}
    for formula in formulae:
        if formula.get("deprecated") or formula.get("disabled"):
            continue
        urls = [formula.get("homepage")] + [(formula.get("urls") or {}).get(kind, {}).get("url") for kind in ("stable", "head")]
        for key in {repository(url) for url in urls} - {None}:
            by_repository.setdefault(key, []).append(formula)
    # An owner rename keeps host and repository name; trusted only when the
    # formula is named after the repository.
    renamed = {forge_name(key): formula for key, found in by_repository.items() for formula in found
               if forge_name(key) and key.rsplit("/", 1)[-1] == formula["name"]}
    programs = {}
    for line in executables.splitlines():
        formula, _, names = line.partition(":")
        programs[re.sub(r"\(.*\)$", "", formula)] = names.split()
    apps, seen = [], set()
    for project, key, section in awesome(readme):
        # One repository can hold several formulae (util-linux); take the one
        # with the project's or repository's name, or the only one. A plain
        # web page identifies a formula only by the project's name.
        found = by_repository.get(key) or [formula for formula in [renamed.get(forge_name(key))] if formula]
        named = [formula for formula in found if formula["name"] in (project.lower(), key.rsplit("/", 1)[-1])]
        formula = named[0] if named else found[0] if len(found) == 1 and key.count("/") == 2 else None
        if not formula or formula["name"] in seen or formula["name"] in SKIP_FORMULAE:
            continue
        run = command(formula["name"], programs.get(formula["name"], []), project)
        if not run:
            continue
        mac, linux = platforms(formula)
        seen.add(formula["name"])
        apps.append({"id": formula["name"], "name": project, "command": run, "description": formula.get("desc") or "",
                     "section": section, "brew": formula["name"], "mac": mac, "linux": linux,
                     "homepage": formula.get("homepage") or ""})
    apps.sort(key=lambda app: (app["section"], app["name"].lower()))
    return {"generated_by": "scripts/prepare-app-catalog.py",
            "sources": {"selection": "https://github.com/rothgar/awesome-tuis at " + revision,
                        "packages": "Homebrew formula index and executables list (homebrew-core, BSD-2-Clause)"},
            "apps": apps}


def fetch(url):
    with urllib.request.urlopen(url, timeout=60) as response:
        return response.read().decode()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--from", dest="cache", type=Path, help="Use awesome-tuis/README.md, formula.json and executables.txt from this folder")
    args = parser.parse_args()
    if args.cache:
        readme = (args.cache / "awesome-tuis/README.md").read_text()
        revision = subprocess.run(["git", "-C", str(args.cache / "awesome-tuis"), "rev-parse", "HEAD"], check=True, capture_output=True, text=True).stdout.strip()
        formulae = json.loads((args.cache / "formula.json").read_text())
        executables = (args.cache / "executables.txt").read_text()
    else:
        with tempfile.TemporaryDirectory() as folder:
            subprocess.run(["git", "clone", "-q", "--depth", "1", AWESOME, folder + "/awesome"], check=True)
            readme = Path(folder, "awesome/README.md").read_text()
            revision = subprocess.run(["git", "-C", folder + "/awesome", "rev-parse", "HEAD"], check=True, capture_output=True, text=True).stdout.strip()
        formulae, executables = json.loads(fetch(FORMULAE)), fetch(EXECUTABLES)
    catalog = generate(readme, formulae, executables, revision)
    OUTPUT.write_text(json.dumps(catalog, indent=1, ensure_ascii=False) + "\n")
    print(f"{len(catalog['apps'])} apps -> {OUTPUT.relative_to(ROOT)}", file=sys.stderr)


if __name__ == "__main__":
    main()
