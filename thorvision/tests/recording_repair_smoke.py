"""Isolated UI confirmation/progress/result and queued SSD identity regressions.

Requires THORVISION_ENABLE_UI_SMOKE=ON. Never connects to a real recording device.
"""
import argparse
import copy
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import re
import subprocess
import threading
import time


def run(executable, output, replacement=False, previous_attachment=None):
    output.mkdir(parents=True, exist_ok=True)
    marker = output/'swap-ready'
    if marker.exists():
        marker.unlink()
    state = {'version': 1, 'revision': 1, 'observed_at': time.time(), 'segment_seconds': 60,
        'busy': False, 'maintenance': None, 'test_operation_count': 0,
        'storage': {'state': 'Repair required', 'reason': 'Filesystem check did not pass',
            'uuid': 'confirmed-volume', 'generation': 'confirmed-attachment',
            'free_bytes': 1996000000000, 'total_bytes': 2000000000000,
            'reserve_bytes': 200000000000, 'repair_enabled': True, 'port_configured': True},
        'cameras': [{'stable_id': 'fixture-camera', 'slot': 0, 'name': 'Camera 1',
            'enabled': True, 'present': True, 'state': 'Repair required', 'reason': '',
            'profile_source': 'saved', 'profile': {'codec': 'h265', 'width': 1920,
                'height': 1080, 'fps_n': 30, 'fps_d': 1}}], 'events': []}
    if previous_attachment:
        state['test_previous_attachment'] = True
        state['storage'].update(state='Ready', reason='')
        state['cameras'][0]['state'] = 'Recording'
        state['maintenance'] = {'id': 'old-ssd-job', 'operation': 'repair',
            'uuid': 'confirmed-volume' if previous_attachment == 'same-volume' else 'previous-volume',
            'generation': 'previous-attachment', 'status': 'completed_with_warnings',
            'phase': 'complete', 'files_total': 1, 'files_recovered': 0, 'files_failed': 1}
        state['events'] = [{'kind': 'SSD recovery completed', 'uuid': state['maintenance']['uuid'],
            'generation': 'previous-attachment', 'failed': 1, 'at': time.time()-3600}]
    operations = []
    operation_at = None
    lock = threading.Lock()

    class Handler(BaseHTTPRequestHandler):
        def send(self, status, value):
            payload = json.dumps(value).encode()
            self.send_response(status)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)

        def do_GET(self):
            if self.path != '/recording':
                self.send(200, [] if self.path == '/cameras' else {'version': '0.1.2', 'status': 'ok'})
                return
            if replacement:
                time.sleep(.5)  # Keep a background read pending when the test submits repair.
            with lock:
                if replacement and marker.exists():
                    state['storage'].update(uuid='replacement-volume', generation='replacement-attachment')
                if operation_at is not None:
                    elapsed = time.monotonic()-operation_at
                    phase = ('stopping_recording' if elapsed < .7 else 'backing_up' if elapsed < 1.5
                        else 'recovering_videos' if elapsed < 4.5 else 'repairing_filesystem' if elapsed < 5.5
                        else 'restoring_videos' if elapsed < 6.5 else 'complete')
                    done = phase == 'complete'
                    state['busy'] = not done
                    state['maintenance'].update(phase=phase, status='completed' if done else 'running',
                        updated_at=time.time(), files_backed_up=int(elapsed >= 1.5),
                        files_processed=int(elapsed >= 4.5), files_recovered=int(done),
                        filesystem_repaired=elapsed >= 5.5, resume_requested=done)
                    if done:
                        state['storage'].update(state='Ready', reason='')
                        state['cameras'][0]['state'] = 'Recording'
                state['observed_at'] = time.time()
                value = copy.deepcopy(state)
            self.send(200, value)

        def do_POST(self):
            nonlocal operation_at
            body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
            with lock:
                operations.append({'path': self.path, 'body': body})
                if replacement:
                    self.send(409, {'detail': 'SSD changed; refresh its state before continuing'})
                    return
                assert self.path == '/recording/storage/repair'
                assert body == {'uuid': 'confirmed-volume', 'generation': 'confirmed-attachment', 'confirmed': True}
                operation_at = time.monotonic()
                state['test_operation_count'] += 1
                state['busy'] = True
                state['maintenance'] = {'id': 'fixture-job', 'operation': 'repair', 'status': 'queued',
                    'phase': 'queued', 'uuid': body['uuid'], 'generation': body['generation'],
                    'started_at': time.time(), 'updated_at': time.time(), 'reason': '',
                    'files_total': 1, 'files_backed_up': 0, 'files_processed': 0,
                    'files_recovered': 0, 'files_failed': 0, 'filesystem_repaired': False,
                    'resume_requested': False, 'files': [{'file': 'Camera 1/interrupted.mkv.part',
                        'status': 'recovered', 'frames': 960, 'duration_seconds': 32.017,
                        'encoded_payload_verified': True, 'codec': 'hevc',
                        'recovered_file': 'Camera 1/recovered.mkv'}]}
                value = copy.deepcopy(state)
            self.send(200, value)

        def log_message(self, *args):
            pass

    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    env = dict(os.environ)
    for key in ['THORVISION_UI_SCREENSHOT', 'THORVISION_PC_SMOKE_DIR', 'THORVISION_UI_SOAK_MS',
                'THORVISION_UI_QUEUE_SMOKE', 'THORVISION_STORAGE_FLOW_DIR', 'THORVISION_REPAIR_FLOW_DIR',
                'THORVISION_UI_ATTACHMENT_SMOKE']:
        env.pop(key, None)
    env.update(THORVISION_UI_SMOKE='1', THORVISION_DEVICE_HOST='127.0.0.1',
               THORVISION_DEVICE_PORT=str(server.server_port), THORVISION_SOFTWARE_PREVIEW='1')
    if replacement:
        env.update(THORVISION_UI_ATTACHMENT_SMOKE='1', THORVISION_UI_SOAK_MS='10000',
                   THORVISION_UI_ATTACHMENT_MARKER=str(marker.resolve()))
    else:
        env['THORVISION_REPAIR_FLOW_DIR'] = str(output.resolve())
    log = output/'application.log'
    try:
        with log.open('w', encoding='utf-8') as stream:
            result = subprocess.run([str(executable), 'debug'], env=env, stdout=stream, stderr=stream,
                                    cwd=executable.parent, timeout=55)
    finally:
        server.shutdown()
        server.server_close()
    logs = log.read_text(encoding='utf-8', errors='replace')
    assert result.returncode == 0, logs[-6000:]
    assert not re.search(r'ReferenceError|TypeError|Cannot assign|is not defined|Binding loop', logs), logs[-6000:]
    assert len(operations) == 1, operations
    assert operations[0]['body'] == {'uuid': 'confirmed-volume', 'generation': 'confirmed-attachment', 'confirmed': True}, operations
    if replacement:
        assert state['storage']['uuid'] == 'replacement-volume'
    else:
        assert 'Repair flow smoke passed' in logs, logs[-6000:]
        assert all((output/name).is_file() for name in ['repair-confirm.png', 'repair-progress.png', 'repair-result.png'])
    (output/'result.json').write_text(json.dumps({'passed': True, 'replacement': replacement, 'previous_attachment': previous_attachment,
        'operations': operations}, indent=2)+'\n', encoding='utf-8')
    print('PASS', 'queued repair retains the confirmed SSD identity' if replacement else 'repair confirmation, progress, result and dismissal')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('executable', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    run(args.executable.resolve(), args.output/'repair-flow')
    run(args.executable.resolve(), args.output/'attachment-change', replacement=True)
    run(args.executable.resolve(), args.output/'same-ssd-reconnected', previous_attachment='same-volume')
    run(args.executable.resolve(), args.output/'replacement-ssd', previous_attachment='different-volume')
