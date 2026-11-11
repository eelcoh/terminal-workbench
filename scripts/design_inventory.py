#!/usr/bin/env python3
"""Design inventory (design step, stage 1): Band beside Rex, task by task.

Builds .local/artifacts/inventory/index.html from the UI test's screenshots
(.local/artifacts/qt-ui/<platform>/*.png). Rex screenshots and Band-on-macOS
screenshots are added by hand as .local/artifacts/inventory/rex/<task>.png and
.local/artifacts/inventory/macos/<task>.png (or <task>-2.png and so on); empty
slots say which file to drop in. Rerun after each UI test or new screenshot.
"""
import html
import os
import sys
from pathlib import Path

# The tasks Rex's screenshots showed (docs/polish.md), with Band's screens for each.
TASKS = [
    ('first-run', 'First run', ['welcome-look', 'welcome-theme', 'welcome-apps', 'welcome-keys']),
    ('sidebar', 'Sidebar: workspaces and panes', ['chrome-workspaces', 'navigation-panes', 'navigation-pinned']),
    ('filter', 'Sidebar filter and kinds', ['navigation-filter', 'navigation-kind-filters', 'navigation-agent-needs']),
    ('hosts', 'Hosts', ['host-chooser', 'host-dialog', 'hosts-reachability', 'host-pane']),
    ('title-bar', 'Title bar and window chrome', ['integrated-title-bar', 'chrome-narrow']),
    ('pane', 'Pane chrome, focused and not', ['pane-actions', 'working']),
    ('commands', 'Commands, menus and keys', ['commands-search', 'chrome-menu', 'chrome-shortcuts']),
    ('settings', 'Settings', ['configuration', 'font-preview', 'font-installed', 'shortcut-editor', 'terminal-colors-preview']),
    ('confirm', 'Confirmations', ['workspace-remove-confirm', 'lifecycle-close-confirm']),
    ('overview', 'Overview', ['overview']),
    ('components', 'Components (gallery)', ['design-gallery', 'design-gallery-in-place']),
]


def shots(folder, task):
    return sorted(p for p in folder.glob(task + '*.png') if p.stem == task or p.stem.startswith(task + '-'))


def figure(path, out, caption):
    src = html.escape(Path(os.path.relpath(path, out)).as_posix())
    return f'<figure><a href="{src}"><img src="{src}" loading="lazy" alt=""></a><figcaption>{html.escape(caption)}</figcaption></figure>'


def column(title, figures, empty):
    body = ''.join(figures) or f'<p class="empty">{html.escape(empty)}</p>'
    return f'<div class="col"><h3>{html.escape(title)}</h3>{body}</div>'


def main():
    root = Path(__file__).resolve().parent.parent
    artifacts = root / '.local' / 'artifacts'
    out = artifacts / 'inventory'
    for name in ('rex', 'macos'):
        (out / name).mkdir(parents=True, exist_ok=True)
    linux = next((artifacts / 'qt-ui' / p for p in ('xcb', 'wayland') if (artifacts / 'qt-ui' / p).is_dir()), None)
    if linux is None:
        sys.exit('No UI test screenshots in .local/artifacts/qt-ui; run just toolkit-ui-test first.')
    missing, sections = [], []
    for task, title, names in TASKS:
        band = []
        for name in names:
            path = linux / (name + '.png')
            if path.exists():
                band.append(figure(path, out, name))
            else:
                missing.append(name)
        mac = [figure(p, out, p.stem) for p in shots(out / 'macos', task)]
        rex = [figure(p, out, p.stem) for p in shots(out / 'rex', task)]
        sections.append(f'<section id="{task}"><h2>{html.escape(title)}</h2><div class="cols">'
                        + column(f'Band · Linux ({linux.name})', band, 'No UI test screenshot yet.')
                        + column('Band · macOS', mac, f'Drop macos/{task}.png here.')
                        + column('Rex', rex, f'Drop rex/{task}.png here.')
                        + '</div></section>')
    page = f'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Design inventory</title>
<style>
:root {{ --bg: #f6f5f2; --fg: #22211f; --muted: #6d6a64; --line: #dcd9d2; }}
@media (prefers-color-scheme: dark) {{ :root {{ --bg: #181816; --fg: #e8e6e1; --muted: #9a968e; --line: #34332f; }} }}
body {{ margin: 0; padding: 24px 16px 64px; background: var(--bg); color: var(--fg); font: 14px/1.45 system-ui, sans-serif; }}
main {{ max-width: 1500px; margin: 0 auto; }}
h1 {{ font-size: 22px; margin: 0 0 4px; }} h2 {{ font-size: 17px; margin: 40px 0 12px; }} h3 {{ font-size: 12px; text-transform: uppercase; letter-spacing: .04em; color: var(--muted); margin: 0 0 8px; }}
nav {{ display: flex; flex-wrap: wrap; gap: 4px 14px; margin: 12px 0 0; }} nav a {{ color: var(--muted); }}
.cols {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 16px; }}
.col {{ border-top: 1px solid var(--line); padding-top: 10px; min-width: 0; }}
figure {{ margin: 0 0 14px; }} img {{ width: 100%; height: auto; border: 1px solid var(--line); border-radius: 6px; display: block; }}
figcaption, .empty, .note {{ color: var(--muted); font-size: 12px; }} figcaption {{ margin-top: 4px; }}
</style></head><body><main>
<h1>Design inventory</h1>
<p class="note">Band beside Rex, task by task (design step, stage 1). Linux screenshots come from the UI test; add Rex and Band-on-macOS screenshots as <code>rex/&lt;task&gt;.png</code> and <code>macos/&lt;task&gt;.png</code> (more as <code>&lt;task&gt;-2.png</code>) in this folder and run <code>just design-inventory</code> again.</p>
<nav>{''.join(f'<a href="#{t}">{html.escape(n)}</a>' for t, n, _ in TASKS)}</nav>
{''.join(sections)}
</main></body></html>
'''
    (out / 'index.html').write_text(page)
    print(out / 'index.html')
    if missing:
        print('Not in the last UI test run: ' + ', '.join(missing))


if __name__ == '__main__':
    main()
