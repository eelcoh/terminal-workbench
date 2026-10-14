#!/usr/bin/env python3
"""Platform dialog contracts, cancellation, arguments and directory validation."""
from pathlib import Path
import os,subprocess,sys,tempfile
from unittest.mock import patch
ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'scripts'))
import folder_picker
from platform_dialogs import macos,linux
from workspace_catalog import directory_value
with tempfile.TemporaryDirectory(prefix='wb-picker-') as temporary:
    root=Path(temporary).resolve();folder=root/'café $quotes \" folder';folder.mkdir()
    assert folder_picker.initial_directory(str(folder/'missing'/'child'),root)==folder
    assert folder_picker.initial_directory(folder.name,root)==folder
    assert folder_picker.initial_directory('',root)==root
    result=subprocess.CompletedProcess([],0,'PICK\t'+str(folder)+'\n','')
    with patch.object(macos.subprocess,'run',return_value=result) as run:
        assert macos.choose(folder)==str(folder)
        assert run.call_args.args[0][-1]==str(folder) and run.call_args.args[0][0]=='/usr/bin/osascript'
        assert not run.call_args.kwargs.get('shell')
    with patch.object(macos.subprocess,'run',return_value=subprocess.CompletedProcess([],0,'CANCEL\n','')):
        assert macos.choose(folder) is None
    for returncode,stdout in ((1,''),(0,'unexpected')):
        with patch.object(macos.subprocess,'run',return_value=subprocess.CompletedProcess([],returncode,stdout,'')):
            try:macos.choose(folder)
            except ValueError:pass
            else:raise AssertionError('Mac failure accepted')
    with patch.object(linux.shutil,'which',return_value='/usr/bin/zenity'),patch.object(linux.subprocess,'run',return_value=subprocess.CompletedProcess([],0,str(folder)+'\n','')) as run:
        assert linux.choose(folder)==str(folder)
        assert '--directory' in run.call_args.args[0] and '--filename='+str(folder)+os.sep in run.call_args.args[0]
        assert not run.call_args.kwargs.get('shell')
    with patch.object(linux.shutil,'which',return_value='/usr/bin/zenity'),patch.object(linux.subprocess,'run',return_value=subprocess.CompletedProcess([],1,'','')):
        assert linux.choose(folder) is None
    with patch.object(linux.shutil,'which',return_value=None):
        try:linux.choose(folder)
        except ValueError as error:assert 'manually' in str(error)
        else:raise AssertionError('missing picker succeeded')
    # A path newline must survive removal of exactly one protocol terminator.
    with patch.object(linux.shutil,'which',return_value='/usr/bin/zenity'),patch.object(linux.subprocess,'run',return_value=subprocess.CompletedProcess([],0,str(folder)+'\n\n','')):
        invalid=linux.choose(folder)
        try:directory_value(invalid,root,available=True)
        except ValueError:pass
        else:raise AssertionError('control-character path accepted')
    for output in ('PICK\t'+str(folder)+'\n','CANCEL\n','ERROR\tproblem\n'):
        with patch.object(linux,'choose',return_value=str(folder) if output.startswith('PICK') else None),patch.object(sys,'argv',['picker','--initial',str(folder),'--cwd',str(root)]):
            if output.startswith('ERROR'):continue
            assert folder_picker.main()==0
print('PASS: native adapter arguments, cancellation/errors, nearest-folder defaults, Unicode/metacharacters and invalid-path isolation')
