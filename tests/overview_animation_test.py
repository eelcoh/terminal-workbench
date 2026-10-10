#!/usr/bin/env python3
"""Compile the pure animation contract without Raylib or a display."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='wb-motion-test-') as directory:
    for name in ('overview_animation', 'input_lookahead', 'catalog_view', 'theme_core'):
        binary = str(Path(directory) / name)
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
            '-I', str(root / 'spikes/ghostling'), str(root / f'tests/{name}_test.c'), '-o', binary], check=True)
        subprocess.run([binary], check=True, env=dict(os.environ, WB_TEST_DIRECTORY=directory))
print('PASS: bounded/reversible animation, reduced motion, stalled frames, queued short presses, Unicode text, bounded lookahead and normal input bypass; cross-workspace pane reuse without reconnect/free')
