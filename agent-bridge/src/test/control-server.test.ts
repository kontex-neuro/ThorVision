import assert from 'node:assert/strict';
import { it } from 'node:test';
import { ControlWorkbench } from '../control-server.js';
import { DeviceClient } from '../device.js';
import { request as httpRequest } from 'node:http';
import { spawn } from 'node:child_process';
import { mkdtempSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createInterface } from 'node:readline';

async function fixture() {
  const device = new DeviceClient('http://127.0.0.1:1/analytics');
  const digest = 'a'.repeat(64), calls: string[] = [];
  const state = { cameras: { cam: { revision: 1, active: { digest }, previous: null } } };
  device.state = async () => structuredClone(state);
  device.packages = async () => ({ packages: [{ digest, ready: true }] });
  device.cameras = async () => [{ stable_id: 'cam', name: 'Test camera' }];
  device.capabilities = async () => ({});
  device.apply = async () => { calls.push('apply'); return { revision: ++state.cameras.cam.revision }; };
  let fingerprint = 'original', externallyPreparing = false, release: () => void = () => {};
  const panel = new ControlWorkbench({ device, baseUrl: device.url, workspace: 'test', camera: '',
    selectCamera: () => {}, fingerprint: () => fingerprint, candidate: () => ({ digest, dirty: false }),
    isPreparing: () => externallyPreparing,
    prepare: async () => { calls.push('prepare'); await new Promise<void>((done) => { release = done; }); return { content: [] }; },
    finalize: async () => { calls.push('finalize'); },
  });
  const connection = await panel.start();
  const request = (path: string, body?: any, headers: Record<string, string> = {}) => fetch(connection.api + '/api/' + path, {
    method: body === undefined ? 'GET' : 'POST', headers: { Authorization: 'Bearer ' + connection.token,
      'Content-Type': 'application/json', ...headers }, body: body === undefined ? undefined : JSON.stringify(body),
  });
  return { panel, connection, request, calls, digest, state, device, edit: () => { fingerprint = 'edited'; }, release: () => release(),
    externalPrepare: () => { externallyPreparing = true; } };
}

it('local controls require a bearer token and reject foreign origins and hosts', async () => {
  const f = await fixture();
  try {
    assert.equal((await fetch(f.connection.api + '/api/status')).status, 401);
    assert.equal((await f.request('status', undefined, { Origin: 'https://attacker.example' })).status, 403);
    const hostStatus = await new Promise((done) => {
      const request = httpRequest(f.connection.api + '/api/status', { headers: { Host: 'attacker.example' } }, (response) => {
        response.resume(); done(response.statusCode);
      }); request.end();
    });
    assert.equal(hostStatus, 403);
    const page = await fetch(f.connection.api);
    assert.equal(page.status, 200);
    assert.match(page.headers.get('content-security-policy')!, /frame-ancestors 'none'/);
    assert.ok(!(await page.text()).includes(f.connection.token));
    assert.equal((await f.request('status')).status, 200);
  } finally { await f.panel.close(); }
});

it('the panel frame endpoint requires the token and reports no frame instead of failing', async () => {
  const f = await fixture();
  try {
    assert.equal((await fetch(f.connection.api + '/api/frame.json')).status, 401);
    const response = await f.request('frame.json');
    assert.equal(response.status, 200);
    assert.deepEqual(await response.json(), { frame: null });
  } finally { await f.panel.close(); }
});

it('selects detected cameras independently of saved disconnected activations and reviews stop for the exact old camera', async () => {
  const f = await fixture();
  const old = { revision: 3, active: { digest: f.digest }, previous: null };
  f.device.state = async () => ({ cameras: { ...f.state.cameras, disconnected: old } });
  try {
    const status = await f.request('status').then((response) => response.json());
    assert.equal(status.camera, 'cam');
    assert.equal((await f.request('camera', { camera: 'disconnected' })).status, 409);
    const response = await f.request('disconnected/review', { camera: 'disconnected' });
    assert.equal(response.status, 200);
    const review = await response.json();
    assert.equal(review.action, 'stop');
    assert.equal(review.camera, 'disconnected');
    assert.equal(review.revision, 3);
    assert.equal((await f.request('status').then((response) => response.json())).camera, 'cam');
    assert.deepEqual(f.calls, []);
    f.device.cameras = async () => [];
    const absent = await f.request('status').then((response) => response.json());
    assert.equal(absent.camera, undefined);
  } finally { await f.panel.close(); }
});

it('a turn edit automatically prepares; unchanged and interrupted turns never prepare or activate', async () => {
  const f = await fixture();
  try {
    await f.request('authoring/begin', {}); await f.request('authoring/finish', {});
    assert.deepEqual(f.calls, []);
    await f.request('authoring/begin', {}); f.edit(); await f.request('authoring/finish', {});
    assert.deepEqual(f.calls, ['finalize']);
    await f.request('authoring/begin', {}); await f.request('authoring/finish', { aborted: true });
    assert.deepEqual(f.calls, ['finalize']);
  } finally { await f.panel.close(); }
});

it('a turn that never finished does not lock out the next turn, and its edits still prepare', async () => {
  const f = await fixture();
  try {
    assert.equal((await f.request('authoring/begin', {})).status, 200);
    f.edit();
    const resumed = await f.request('authoring/begin', {});
    assert.equal(resumed.status, 200);
    assert.equal((await resumed.json()).resumed, true);
    await f.request('authoring/finish', {});
    assert.deepEqual(f.calls, ['finalize']);
  } finally { await f.panel.close(); }
});

it('preparation is single flight and prevents reviewed device mutations while in progress', async () => {
  const f = await fixture();
  try {
    await f.request('prepare', {}); await f.request('prepare', {});
    assert.deepEqual(f.calls, ['prepare']);
    assert.equal((await f.request('camera', { camera: 'cam' })).status, 409);
    assert.equal((await f.request('authoring/begin', {})).status, 409);
    f.release();
    assert.equal((await f.request('status').then((r) => r.json())).busy, false);
  } finally { f.release(); await f.panel.close(); }
});

it('human review never activates until executed, and stale reviews cannot activate', async () => {
  const f = await fixture();
  try {
    await f.request('camera', { camera: 'cam' });
    const review = await f.request('review', { action: 'apply', digest: f.digest }).then((r) => r.json());
    assert.deepEqual(f.calls, []);
    f.state.cameras.cam.revision++;
    assert.equal((await f.request('execute', { id: review.id })).status, 202);
    assert.match((await f.request('status').then((r) => r.json())).operation.message, /activation changed/);
    assert.deepEqual(f.calls, []);
    const current = await f.request('review', { action: 'apply', digest: f.digest }).then((r) => r.json());
    assert.equal((await f.request('execute', { id: current.id })).status, 202);
    assert.deepEqual(f.calls, ['apply']);
  } finally { await f.panel.close(); }
});

it('agent-owned preparation blocks editing without offering a false panel cancellation', async () => {
  const f = await fixture();
  try {
    f.externalPrepare();
    const status = await f.request('status').then((r) => r.json());
    assert.equal(status.busy, true);
    assert.equal(status.canCancel, false);
    assert.equal((await f.request('authoring/begin', {})).status, 409);
    assert.equal((await f.request('prepare', {})).status, 409);
    assert.equal((await f.request('cancel', {})).status, 409);
    assert.deepEqual(f.calls, []);
  } finally { await f.panel.close(); }
});

it('authoring reserves ownership before awaiting device discovery', async () => {
  const f = await fixture();
  let entered: () => void = () => {}, release: () => void = () => {};
  const observed = new Promise<void>((done) => { entered = done; });
  const delayed = new Promise<void>((done) => { release = done; });
  const readState = f.device.state.bind(f.device);
  f.device.state = async () => { entered(); await delayed; return readState(); };
  try {
    const begin = f.request('authoring/begin', {});
    await observed;
    assert.equal(f.panel.changingDevice, true);
    assert.equal((await f.request('execute', { id: 'anything' })).status, 409);
    assert.equal((await f.request('prepare', {})).status, 409);
    assert.equal((await f.request('camera', { camera: 'cam' })).status, 409);
    release();
    assert.equal((await begin).status, 200);
    assert.deepEqual(f.calls, []);
  } finally { release(); await f.panel.close(); }
});

it('the local live-view host exits when its native MCP owner closes stdin', async () => {
  const child = spawn(process.execPath, [fileURLToPath(new URL('../tools/server.js', import.meta.url))], {
    env: { ...process.env, THORVISION_WORKSPACE: mkdtempSync(join(tmpdir(), 'tv-owner-')),
      THORVISION_EXTENSIONS_URL: 'http://127.0.0.1:1/analytics', THORVISION_CAMERA: '' }, stdio: 'pipe',
  });
  const lines = createInterface({ input: child.stdout });
  const send = (message: unknown) => child.stdin.write(JSON.stringify(message) + '\n');
  const replies = new Map<number, (message: any) => void>();
  lines.on('line', (line) => { const message = JSON.parse(line); replies.get(message.id)?.(message); });
  const rpc = (id: number, method: string, params: unknown) => new Promise<any>((done) => {
    replies.set(id, done); send({ jsonrpc: '2.0', id, method, params });
  });
  let timer: ReturnType<typeof setTimeout> | undefined;
  const deadline = new Promise<never>((_, reject) => { timer = setTimeout(() => reject(new Error('MCP owner close timed out')), 5000); });
  try {
    await Promise.race([rpc(1, 'initialize', { protocolVersion: '2025-11-25', capabilities: {}, clientInfo: { name: 'lifecycle-test', version: '1' } }), deadline]);
    send({ jsonrpc: '2.0', method: 'notifications/initialized' });
    const reply = await Promise.race([rpc(2, 'tools/call', { name: 'open_control_panel', arguments: {} }), deadline]);
    const connection = JSON.parse(reply.result.content[0].text);
    assert.equal((await fetch(connection.api)).status, 200);
    const exit = new Promise((done) => child.once('exit', done));
    child.stdin.end();
    assert.equal(await Promise.race([exit, deadline]), 0);
    await assert.rejects(fetch(connection.api));
  } finally { clearTimeout(timer); lines.close(); child.kill(); }
});
