"""Ensure this feature stays updater-only and directly rooted in production qml."""
from pathlib import Path
import json,subprocess
root=Path(__file__).resolve().parents[1]
base='1523013fd5fbe598c35569676c98f7a212c6552c'
def git(*args):return subprocess.check_output(['git','-C',str(root),*args])
allowed={'CMakeLists.txt','thorvision/CMakeLists.txt','thorvision/src/UpdateController.cc',
         'thorvision/src/UpdateController.h','thorvision/src/DeviceUpdateSelection.h',
         'thorvision/tests/updater_selection_test.cc','docs/updater-qml.md',
         'scripts/check_updater_release_scope.py'}
names=set(git('diff','--name-only',base).decode().splitlines())
untracked=set(git('ls-files','--others','--exclude-standard').decode().splitlines())
assert names|untracked <= allowed, sorted((names|untracked)-allowed)
assert not (root/'agent-bridge').exists()
assert not git('diff',base,'--','thorvision/ui','resources','.github')
assert subprocess.run(['git','-C',str(root),'merge-base','--is-ancestor',base,'HEAD']).returncode==0
for forbidden in ['0632765032927b9a135e0361e47958d66af8f13b','6a9d3290df58e3a9e0992998f8e5613fba91a19b']:
    assert subprocess.run(['git','-C',str(root),'merge-base','--is-ancestor',forbidden,'HEAD']).returncode==1
print(json.dumps({'base':base,'updater_only':True,'gui_camera_recording_publish_diff':'empty','changed_files':sorted(names|untracked)},indent=2))
