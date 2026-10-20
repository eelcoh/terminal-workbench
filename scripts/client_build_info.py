"""Capture client build identity once; never inspect terminal or workspace data."""
import json
from pathlib import Path
import sys


def capture(root,*,installed=False):
    result=dict(kind='installed preview' if installed else 'development checkout',payload_root=str(root),python=sys.executable)
    path=Path(root)/'preview.json'
    if not path.exists():
        if installed:result['metadata_error']='Preview metadata is missing'
        return result
    try:
        metadata=json.loads(path.read_text())
        if not isinstance(metadata,dict) or metadata.get('format')!=1:raise ValueError('Unsupported preview metadata')
        revision=metadata.get('source')
        if not isinstance(revision,str) or len(revision)!=40 or any(c not in '0123456789abcdef' for c in revision):
            raise ValueError('Invalid preview source revision')
        result.update(kind='installed preview' if installed else 'staged preview',revision=revision,
            source_dirty=metadata.get('source_dirty') is True,qt_bundled=metadata.get('qt_bundled') is True,
            selective_qml=metadata.get('selective_qml') is True)
    except (OSError,ValueError) as error:result['metadata_error']=str(error)
    return result
