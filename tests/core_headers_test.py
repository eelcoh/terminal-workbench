#!/usr/bin/env python3
"""Compile the Qt client's pure C core contracts without Qt or a display."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='wb-core-test-') as directory:
    for name in ('attention_navigation', 'terminal_search', 'text_edit', 'theme_core', 'shortcut_state', 'portable_config', 'font_preferences'):
        binary = str(Path(directory) / name)
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
            '-I', str(root / 'spikes/qt-input/core'), str(root / f'tests/{name}_test.c'), '-o', binary, '-lm'], check=True)
        subprocess.run([binary], check=True, env=dict(os.environ, WB_TEST_DIRECTORY=directory))
print('PASS: waiting-agent order, Unicode search and text editing, theme files, shortcut state, portable settings and font preferences')
