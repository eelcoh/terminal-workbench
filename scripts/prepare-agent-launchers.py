#!/usr/bin/env python3
"""Generate local PATH entries without changing shell or agent configuration."""
from pathlib import Path
import shlex

root = Path(__file__).resolve().parent.parent
directory = root / '.local/bin'
directory.mkdir(parents=True, exist_ok=True)
for agent in ('codex', 'claude'):
    path = directory / f'wb-{agent}'
    contents = '#!/bin/sh\nexec python3 ' + shlex.quote(str(root / 'scripts/workbench-agent.py')) + ' ' + agent + ' "$@"\n'
    if not path.exists() or path.read_text() != contents:
        path.write_text(contents)
    path.chmod(0o755)
