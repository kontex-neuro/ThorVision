import assert from 'node:assert/strict';
import { cpSync, mkdtempSync, readFileSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { describe, it } from 'node:test';
import { DeviceClient } from '../device.js';
import { SDK_SNAPSHOT, INSTRUCTIONS } from '../adapters/common.js';
import { finalizeProject, prepareUploaded } from '../preparation.js';
import { seal } from '../package.js';
import { prepareExtension } from '../tools/server.js';

function fixture() {
  const workspace = mkdtempSync(join(tmpdir(), 'tv-preparation-'));
  const directory = join(workspace, 'package');
  cpSync(join(SDK_SNAPSHOT, 'examples', 'moving_green_box'), directory, { recursive: true });
  const calls: string[] = [];
  const device = new DeviceClient('http://127.0.0.1:1');
  let failure = '';
  device.upload = async () => { calls.push('upload'); return { digest: seal(directory) }; };
  device.job = async (kind, digest) => {
    calls.push(kind);
    return failure === kind ? { state: 'failed', report: { passed: false, digest, error: `${kind} failed precisely` } }
      : { state: 'succeeded', report: { passed: true, digest, frames: kind === 'test' ? 12 : undefined, cached: kind === 'prepare' } };
  };
  return { context: { workspace, camera: 'camera', snapshot: SDK_SNAPSHOT, device }, calls, directory,
    fail: (kind: string) => { failure = kind; } };
}

describe('automatic preparation', () => {
  it('validates, reuses/prepares and tests, and avoids another test after fresh readiness', async () => {
    const f = fixture();
    const startedAt = Date.now();
    await finalizeProject(f.context, startedAt, () => {}, new AbortController().signal);
    assert.deepEqual(f.calls, ['upload', 'prepare', 'upload', 'test']);
    await finalizeProject(f.context, startedAt, () => {}, new AbortController().signal);
    assert.equal(f.calls.length, 4);
    writeFileSync(join(f.directory, 'processor.py'), readFileSync(join(f.directory, 'processor.py'), 'utf8') + '\n# revised\n');
    await finalizeProject(f.context, startedAt, () => {}, new AbortController().signal);
    assert.equal(f.calls.filter((call) => call === 'test').length, 2);
  });

  it('never tests after failed preparation and preserves the exact failure', async () => {
    const f = fixture();
    f.fail('prepare');
    await assert.rejects(finalizeProject(f.context, Date.now(), () => {}, new AbortController().signal), /prepare failed precisely/);
    assert.deepEqual(f.calls, ['upload', 'prepare']);
  });

  it('does not mark a failed test ready', async () => {
    const f = fixture();
    f.fail('test');
    const result = await prepareExtension(f.context);
    assert.equal(result.isError, true);
    assert.match(JSON.stringify(result), /test failed precisely/);
  });

  it('does not retry a failure the agent already received for this revision', async () => {
    const f = fixture();
    const startedAt = Date.now() - 1;
    f.fail('prepare');
    assert.equal((await prepareExtension(f.context)).isError, true);
    const previousCalls = [...f.calls];
    await assert.rejects(finalizeProject(f.context, startedAt, () => {}, new AbortController().signal), /prepare failed precisely/);
    assert.deepEqual(f.calls, previousCalls);
  });

  it('rejects invalid manifests before any upload', async () => {
    const f = fixture();
    writeFileSync(join(f.directory, 'manifest.json'), '{}');
    assert.equal((await prepareExtension(f.context)).isError, true);
    assert.deepEqual(f.calls, []);
  });

  it('cancels before preparation starts', async () => {
    const f = fixture();
    const controller = new AbortController();
    controller.abort();
    await assert.rejects(prepareExtension(f.context, () => {}, controller.signal), /abort/i);
    assert.deepEqual(f.calls, []);
  });

  it('prepares uploaded packages without an agent or subscription', async () => {
    const f = fixture();
    const result: any = await prepareUploaded(f.context.device, seal(f.directory), () => {}, new AbortController().signal);
    assert.equal(result.ready, true);
    assert.deepEqual(f.calls, ['prepare', 'test']);
  });

  it('shares mandatory preparation instructions across providers', () => {
    assert.match(INSTRUCTIONS, /prepare_extension/);
    assert.match(INSTRUCTIONS, /never ask the user to specify dependency reuse/);
  });

  it('accepts the device API but rejects credentials, public targets and redirects-by-URL', () => {
    assert.doesNotThrow(() => new DeviceClient('http://192.168.177.100:8000/analytics'));
    for (const url of ['http://example.com/analytics', 'http://8.8.8.8/analytics', 'http://user:pass@192.168.1.2/analytics',
      'http://192.168.1.2/other', 'http://192.168.1.2/analytics?next=http://other'])
      assert.throws(() => new DeviceClient(url));
  });
});
