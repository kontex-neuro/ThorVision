import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { it } from 'node:test';
import { runInNewContext } from 'node:vm';

const source = readFileSync(new URL('../../control-ui/app.js', import.meta.url), 'utf8')
  .replace(/pollStatus\(\); pollFrame\(\);\s*$/, '');
const camera = '/dev/v4l/by-path/current', disconnected = '/dev/v4l/by-path/old';
const digest = 'a'.repeat(64);

function fixture(options: { modern?: boolean; wrongCamera?: boolean; reviewError?: boolean } = {}) {
  const calls: { path: string; body?: { camera?: string; action?: string } }[] = [];
  const snapshot = { camera, cameras: [
    { id: camera, discovered: true, state: {} },
    { id: disconnected, discovered: false, state: { active: { digest } } },
  ], operation: { state: 'failed', message: `analytics is already active on ${disconnected}` },
  hasProject: true, candidate: { digest, dirty: false } };
  const ui: {
    reviewDisconnected(camera: string): Promise<void>;
    guide(value: typeof snapshot, candidate: { ready: boolean }, state: { active?: { digest: string } }): { title: string; message: string; blocked?: boolean; action?: string; starters?: [string, string][] };
    setup(value: typeof snapshot): void;
    pending(): { camera: string; action: string } | undefined;
    health(value: object, state: object): { word: string; tone: string; warning?: string };
    frameTime(value: number): void;
  } = runInNewContext(source + '\n({ reviewDisconnected, guide, health, frameTime: (value) => { lastFrame = value; }, setup: (value) => { snapshot = value; }, pending: () => pending })', {
    URLSearchParams, AbortSignal, location: { search: '', hash: '' },
    sessionStorage: { getItem: () => 'test-token' },
    document: { getElementById: () => ({}), addEventListener() {} },
    fetch: async (path: string, init: RequestInit) => {
      const body = typeof init.body === 'string' ? JSON.parse(init.body) : undefined;
      calls.push({ path, body });
      if (path === '/api/disconnected/review' && !options.modern) return Response.json({ error: 'Unknown operation' }, { status: 404 });
      if (path === '/api/review' && options.reviewError) return Response.json({ error: 'camera activation changed' }, { status: 409 });
      return Response.json(path === '/api/camera' ? { camera: body.camera }
        : { action: 'stop', camera: options.wrongCamera ? camera : disconnected, revision: 1, digest });
    },
  });
  ui.setup(snapshot);
  return { ui, snapshot, calls };
}

it('the current server reviews disconnected stop directly without changing selection or executing', async () => {
  const f = fixture({ modern: true });
  await f.ui.reviewDisconnected(disconnected);
  assert.deepEqual(f.calls.map((call) => call.path), ['/api/disconnected/review']);
  assert.equal(f.ui.pending()?.camera, disconnected);
  assert.equal(f.ui.pending()?.action, 'stop');
});

it('live camera preview is healthy without an analysis or a prepared project', () => {
  const f = fixture();
  f.ui.frameTime(Date.now());
  assert.equal(f.ui.health({ camera, state: { raw_previews: {} }, preview: { kind: 'raw' } }, {}).word, 'Camera live');
});

it('raw camera faults are shown instead of an idle or healthy detector', () => {
  const f = fixture();
  const health = f.ui.health({ camera, state: { raw_previews: { [camera]: { fault: { message: 'camera disconnected' } } } } }, {});
  assert.equal(health.word, 'Camera unavailable');
  assert.equal(health.warning, 'camera disconnected');
  assert.equal(health.tone, 'error');
});

it('the legacy server reviews the exact disconnected camera and restores the detected selection without executing', async () => {
  const f = fixture();
  await f.ui.reviewDisconnected(disconnected);
  assert.deepEqual(f.calls, [
    { path: '/api/disconnected/review', body: { camera: disconnected } },
    { path: '/api/camera', body: { camera: disconnected } },
    { path: '/api/review', body: { action: 'stop' } },
    { path: '/api/camera', body: { camera } },
  ]);
  assert.equal(f.ui.pending()?.camera, disconnected);
});

it('a legacy review failure still restores selection and a mismatched review is never presented', async () => {
  for (const options of [{ reviewError: true }, { wrongCamera: true }]) {
    const f = fixture(options);
    await assert.rejects(f.ui.reviewDisconnected(disconnected), /activation changed|camera changed/);
    assert.deepEqual(f.calls.at(-1), { path: '/api/camera', body: { camera } });
    assert.equal(f.ui.pending(), undefined);
    assert.ok(!f.calls.some((call) => call.path === '/api/execute'));
  }
});

it('the known disconnected-slot conflict directs stop first instead of offering another run', () => {
  const f = fixture();
  const step = f.ui.guide(f.snapshot, { ready: true }, {});
  assert.equal(step.blocked, true);
  assert.equal(step.action, undefined);
  assert.match(step.title, /Stop the disconnected/);
  assert.match(step.message, /then run/);
  f.snapshot.operation.message = 'some other operation failed';
  const failed = f.ui.guide(f.snapshot, { ready: true }, {});
  assert.equal(failed.blocked, undefined);
  assert.match(failed.message, /No analysis is running on this camera/);
  assert.ok(!failed.message.includes('keeps running'));
});

it('the demo WebUI offers exactly its two frozen recipe prompts in empty and running projects', () => {
  const f = fixture();
  const options = [
    { id: 'person-detector', label: 'Person detector', prompt: 'Restore exact bundled person-detector.' },
    { id: 'three-zone-entries', label: 'Three-zone entries', prompt: 'Restore exact bundled three-zone-entries.' },
  ];
  const snapshot = { ...f.snapshot, operation: { state: 'idle', message: '' }, hasProject: false, demo: { options } };
  for (const state of [{}, { active: { digest } }]) {
    const step = f.ui.guide(snapshot, { ready: true }, state);
    assert.equal(step.title, 'Choose a demo');
    assert.deepEqual(Array.from(step.starters ?? [], (item) => Array.from(item)), options.map((item) => [item.label, item.prompt]));
    assert.equal(step.starters?.length, 2);
  }
});
