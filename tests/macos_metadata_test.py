#!/usr/bin/env python3
"""Check Cocoa display metadata and preparation/linker wiring."""
from pathlib import Path
import plistlib
root=Path(__file__).resolve().parent.parent
source=root/'spikes/ghostling/macos-Info.plist'
metadata=plistlib.loads(source.read_bytes())
assert metadata['CFBundleDisplayName']=='Terminal Workbench'
assert metadata['CFBundleName']=='Terminal Workbench'
assert metadata['CFBundleExecutable']=='ghostling'
assert metadata['NSHighResolutionCapable'] is True
# Do not run preparation here: it downloads/builds and mutates files.
generator=(root/'scripts/prepare-ghostling.py').read_text()
assert '"macos-Info.plist"' in generator
assert 'LINKER:-sectcreate,__TEXT,__info_plist,${CMAKE_SOURCE_DIR}/macos-Info.plist' in generator
assert 'LINK_DEPENDS "${CMAKE_SOURCE_DIR}/macos-Info.plist"' in generator
assert 'if(APPLE)' in generator
print('PASS: Mac display metadata and Apple linker wiring configured, executable identity retained')
