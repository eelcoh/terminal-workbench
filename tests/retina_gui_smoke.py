"""Exercise backing resolution on Linux; this does not simulate native Mac font appearance."""
from pathlib import Path
import json, os, subprocess, sys, tempfile
root=Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='wb-pixel-geometry-') as directory:
    metrics=[]
    for scale in (1,2):
        path=Path(directory)/str(scale)
        env=dict(os.environ,WORKBENCH_RENDER_SCALE=str(scale),WORKBENCH_GEOMETRY_OUTPUT=str(path))
        subprocess.run([sys.executable,str(root/'tests/ui_focus_gui_smoke.py'),*sys.argv[1:]],env=env,check=True)
        metrics.append(json.loads(path.read_text()))
    assert metrics[0]==metrics[1], f'Backing resolution changed PTY geometry: {metrics}'
print(f'PASS: 1x/2x backing textures and atlases retain identical logical PTY rows/columns {metrics[0]}')
