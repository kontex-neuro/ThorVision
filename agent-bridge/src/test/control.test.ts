import assert from 'node:assert/strict';
import { describe, it } from 'node:test';
import { ControlSession } from '../control.js';
import { DeviceClient, DeviceError } from '../device.js';

const CAMERA = '/dev/v4l/by-path/usb-test';
const FIRST = 'a'.repeat(64);
const SECOND = 'b'.repeat(64);

function fixture() {
  const device = new DeviceClient('http://127.0.0.1:1/analytics');
  const entry = { revision: 4, active: { digest: FIRST, config: {} }, previous: { digest: SECOND, config: {} } };
  const state: any = { cameras: { [CAMERA]: entry } };
  const catalog = { packages: [FIRST, SECOND].map((digest) => ({ digest, ready: true, can_delete: false })) };
  const calls: any[] = [];
  let now = 100000;
  let blocked: any[] = [];
  device.state = async () => structuredClone(state);
  device.packages = async () => structuredClone(catalog);
  device.capabilities = async () => ({ lifecycle: { cleanup: true } });
  device.cameras = async () => [{ stable_id: CAMERA, name: 'Desk camera', id: 0 }];
  device.apply = async (...args) => { calls.push(['apply', ...args]); return { revision: ++entry.revision }; };
  device.stop = async (...args) => { calls.push(['stop', ...args]); return { revision: ++entry.revision }; };
  device.cleanupPlan = async (packages, cache) => ({ id: 'backend-plan', packages,
    include_unused_cache: cache, blocked, expires_at: (now + 300000) / 1000, items: [], total_bytes: 0 });
  device.cleanupExecute = async (id) => { calls.push(['cleanup', id]); return { state: 'completed' }; };
  const session = new ControlSession(device, { camera: CAMERA, workspace: '/project', now: () => now });
  return { device, entry, state, catalog, calls, session, advance: () => { now += 300001; },
    block: () => { blocked = [{ digest: FIRST, reasons: ['active'] }]; } };
}

describe('reviewed standalone controls', () => {
  it('discovers stable camera identities and names, never substitutes numeric IDs', async () => {
    const f = fixture();
    const snapshot = await f.session.snapshot();
    assert.equal(snapshot.cameras[0].id, CAMERA);
    assert.equal(snapshot.cameras[0].name, 'Desk camera');
    f.state.cameras['old-empty-request'] = { revision: 0 };
    assert.equal((await f.session.cameras()).length, 1);
    await assert.rejects(f.session.selectCamera('0'), /catalog/);
    f.device.cameras = async () => { throw new Error('camera API unavailable'); };
    const fallback = await f.session.snapshot();
    assert.equal(fallback.cameras[0].id, CAMERA);
    assert.equal(fallback.cameras[0].discovered, false);
    assert.match(fallback.cameraDiscoveryError!, /unavailable/);
  });

  it('binds exact digest, camera, revision, empty config and idempotency key without mutating during review', async () => {
    const f = fixture();
    const review = await f.session.review({ action: 'apply', digest: SECOND });
    const originalId = review.id;
    assert.deepEqual(f.calls, []);
    review.digest = FIRST;
    review.camera = 'caller changed';
    review.config.changed = true;
    const [one, two] = await Promise.all([f.session.execute(originalId), f.session.execute(originalId)]);
    assert.deepEqual(one, two);
    assert.deepEqual(f.calls, [['apply', CAMERA, SECOND, {}, 4, originalId]]);
  });

  it('rejects stale revisions or changed activation even if revision is reused', async () => {
    const f = fixture();
    const old = await f.session.review({ action: 'stop' });
    f.entry.revision++;
    await assert.rejects(f.session.execute(old.id), /activation changed/);
    const newer = await f.session.review({ action: 'stop' });
    f.entry.active.digest = SECOND;
    await assert.rejects(f.session.execute(newer.id), /activation changed/);
    assert.deepEqual(f.calls, []);
  });

  it('requires server ready both at review and execution, not a historical passing report', async () => {
    const f = fixture();
    f.catalog.packages[1].ready = false;
    await assert.rejects(f.session.review({ action: 'apply', digest: SECOND }), /not ready/);
    f.catalog.packages[1].ready = true;
    const review = await f.session.review({ action: 'apply', digest: SECOND });
    f.catalog.packages[1].ready = false;
    await assert.rejects(f.session.execute(review.id), /readiness changed/);
    assert.deepEqual(f.calls, []);
  });

  it('rolls back only to reviewed previous digest and stops only reviewed camera revision', async () => {
    const f = fixture();
    const rollback = await f.session.review({ action: 'rollback', digest: FIRST });
    assert.equal(rollback.digest, SECOND);
    await f.session.execute(rollback.id);
    const stop = await f.session.review({ action: 'stop' });
    await f.session.execute(stop.id);
    assert.equal(f.calls[0][2], SECOND);
    assert.deepEqual(f.calls[1], ['stop', CAMERA, 5]);
  });

  it('rejects unknown cameras, malformed digests, absent and expired reviews', async () => {
    const f = fixture();
    await assert.rejects(f.session.review({ action: 'apply', camera: 'other', digest: SECOND }), /select a camera/);
    await assert.rejects(f.session.review({ action: 'apply', digest: 'latest' }), /exact package/);
    await assert.rejects(f.session.execute('invented'), /missing/);
    const review = await f.session.review({ action: 'stop' });
    f.advance();
    await assert.rejects(f.session.execute(review.id), /expired/);
  });

  it('refuses protected cleanup, binds exact plan and lets backend reject changed references', async () => {
    const f = fixture();
    f.block();
    const blocked = await f.session.reviewCleanup([FIRST]);
    blocked.plan.blocked = [];
    await assert.rejects(f.session.execute(blocked.id), /protected/);
    assert.deepEqual(f.calls, []);
    const other = fixture();
    const review = await other.session.reviewCleanup([SECOND], true);
    other.device.cleanupExecute = async () => { throw new DeviceError(409, 'analytics store changed'); };
    await assert.rejects(other.session.execute(review.id), /store changed/);
    other.device.cleanupExecute = async () => { throw new Error('must not retry'); };
    await assert.rejects(other.session.execute(review.id), /store changed/);
  });

  it('executes only the saved backend cleanup plan and does not implicitly select packages', async () => {
    const f = fixture();
    const review = await f.session.reviewCleanup([], true);
    assert.deepEqual(review.plan.packages, []);
    review.plan.id = 'changed-by-caller';
    await f.session.execute(review.id);
    assert.deepEqual(f.calls, [['cleanup', 'backend-plan']]);
  });

  it('preserves revision/idempotency headers and encoded camera IDs in HTTP request construction', async (t) => {
    const requests: { url: string; options?: RequestInit }[] = [];
    t.mock.method(globalThis, 'fetch', async (url: string, options?: RequestInit) => {
      requests.push({ url, options });
      return new Response(JSON.stringify(url.endsWith('/cameras') ? [] : { revision: 5 }), { status: 200 });
    });
    const device = new DeviceClient('http://192.168.177.100:8000/analytics');
    await device.apply(CAMERA, SECOND, {}, 4, 'review-id');
    await device.stop(CAMERA, 5);
    await device.cameras();
    assert.equal(requests[0].url, 'http://192.168.177.100:8000/analytics/v1/cameras/' + encodeURIComponent(CAMERA) + '/extension');
    assert.equal((requests[0].options?.headers as any)['Idempotency-Key'], 'review-id');
    assert.deepEqual(JSON.parse(requests[0].options?.body as string), { digest: SECOND, config: {}, revision: 4 });
    assert.ok(requests[1].url.endsWith('/extension?revision=5'));
    assert.equal(requests[2].url, 'http://192.168.177.100:8000/cameras');
  });
});
