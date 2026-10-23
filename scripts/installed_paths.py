"""User-owned state for the staged Qt preview, separate from checkout sessions."""
from pathlib import Path


def session_directory(environment):
    if environment.get('BAND_STATE_DIR'):
        return Path(environment['BAND_STATE_DIR']).expanduser().resolve()
    home=Path(environment.get('HOME',str(Path.home())))
    base=Path(environment.get('XDG_STATE_HOME',str(home/'.local/state'))).expanduser()
    if not base.is_absolute():
        raise ValueError('XDG_STATE_HOME must be an absolute path')
    return (base/'band').resolve()
