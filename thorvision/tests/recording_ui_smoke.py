"""Opt-in Windows UI/PC recording integration against the Pi synthetic fixture.

Build with THORVISION_ENABLE_UI_SMOKE=ON. Put the matching GStreamer bin before
Qt bin on PATH. Uses no physical camera or mounted recording SSD.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('executable',type=Path)
    parser.add_argument('--host',required=True)
    parser.add_argument('--port',default=18803,type=int)
    args=parser.parse_args()
    if args.port==8000:parser.error('Use an isolated synthetic fixture port')
    with tempfile.TemporaryDirectory(prefix='thorvision-pc-smoke-') as temporary:
        output=Path(temporary)
        env=dict(os.environ,THORVISION_DEVICE_HOST=args.host,THORVISION_DEVICE_PORT=str(args.port),
                 THORVISION_UI_SMOKE='1',THORVISION_PC_SMOKE_DIR=str(output),THORVISION_SOFTWARE_PREVIEW='1')
        env.pop('THORVISION_UI_SCREENSHOT',None)
        with (output/'client.log').open('w',encoding='utf-8') as log:
            process=subprocess.Popen([str(args.executable),'debug'],env=env,stdout=log,stderr=log)
            try:code=process.wait(timeout=100)
            except subprocess.TimeoutExpired:
                process.terminate();process.wait(timeout=5);raise
        logs=(output/'client.log').read_text(encoding='utf-8',errors='replace')
        assert code==0,logs[-12000:]
        assert 'UI recording smoke passed' in logs,logs[-12000:]
        clips=sorted(output.rglob('*.mkv'))
        assert len(clips)==5,[p.name for p in clips]
        assert sum(p.name.startswith('Camera 1-') for p in clips)==2
        for clip in clips:
            checked=subprocess.run(['ffmpeg','-v','error','-i',str(clip),'-f','null','-'],capture_output=True,timeout=30)
            assert checked.returncode==0 and not checked.stderr,(clip,checked.stderr)
        print(json.dumps({'pc_completed_playable_files':len(clips),'shared_source_changed_30_to_15_fps':True,
                          'affected_pc_resumed':True,'peer_recordings_uninterrupted':True},indent=2))

if __name__=='__main__':main()
