// Package digests (parity with thorvision_sdk.package) and the ThorVision MCP tools against a fake device.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { cpSync, mkdirSync, mkdtempSync, readFileSync, symlinkSync, writeFileSync } from 'node:fs';
import { createServer, type Server } from 'node:http';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { gunzipSync } from 'node:zlib';
import { after, before, describe, it } from 'node:test';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import { SDK_SNAPSHOT } from '../adapters/common.js';
import { DeviceClient } from '../device.js';
import { contentDigest, packTarGz, packageDigest, pythonJson, satisfies, seal, validateManifest } from '../package.js';
import { createServer as createTools, projectPath, type ToolContext } from '../tools/server.js';

const EXAMPLE = join(SDK_SNAPSHOT, 'examples', 'moving_green_box');

/** Minimal ustar reader for checking what the device receives. */
function untar(archive: Buffer): Map<string, Buffer> {
  const data = gunzipSync(archive);
  const files = new Map<string, Buffer>();
  for (let offset = 0; offset + 512 <= data.length;) {
    const header = data.subarray(offset, offset + 512);
    if (header.every((byte) => byte === 0)) break;
    const field = (start: number, length: number) => header.subarray(start, start + length).toString().replace(/\0.*$/s, '');
    const name = [field(345, 155), field(0, 100)].filter(Boolean).join('/');
    const size = Number.parseInt(field(124, 12), 8);
    let checksum = 0;
    header.forEach((byte, index) => (checksum += index >= 148 && index < 156 ? 32 : byte));
    assert.equal(Number.parseInt(field(148, 8), 8), checksum, `${name} header checksum`);
    files.set(name, data.subarray(offset + 512, offset + 512 + size));
    offset += 512 + Math.ceil(size / 512) * 512;
  }
  return files;
}

describe('package digests', () => {
  it('match the digests sealed by the Python SDK', () => {
    const manifest = JSON.parse(readFileSync(join(EXAMPLE, 'manifest.json'), 'utf8'));
    assert.equal(contentDigest(EXAMPLE), manifest.content_digest);
    assert.equal(packageDigest(EXAMPLE), '17e4cd7d3dd6c60eab44c75d2267c07500688973304425f4726b57912ef07fbe');
  });

  it('re-sealing reproduces the Python manifest bytes, and any edit changes the digest', () => {
    const copy = mkdtempSync(join(tmpdir(), 'tv-pkg-'));
    cpSync(EXAMPLE, copy, { recursive: true });
    assert.equal(seal(copy), packageDigest(EXAMPLE));
    writeFileSync(join(copy, 'processor.py'), readFileSync(join(copy, 'processor.py'), 'utf8') + '\n# edit\n');
    assert.notEqual(seal(copy), packageDigest(EXAMPLE));
    assert.equal(pythonJson({ b: [1, 'é'], a: {} }), '{\n  "a": {},\n  "b": [\n    1,\n    "\\u00e9"\n  ]\n}');
  });

  it('validates manifests like the device and packs a tar the device can read', () => {
    assert.deepEqual(validateManifest(EXAMPLE, '0.1.0').problems, []);
    assert.match(validateManifest(EXAMPLE, '0.2.0').problems.join(), /requires SDK/);
    assert.ok(satisfies('0.1.5', '>=0.1,<0.2') && !satisfies('0.2', '>=0.1,<0.2'));
    const files = untar(packTarGz(EXAMPLE));
    assert.deepEqual([...files.keys()].sort(), ['config.schema.json', 'manifest.json', 'processor.py']);
    assert.deepEqual(files.get('manifest.json'), readFileSync(join(EXAMPLE, 'manifest.json')));
  });
});

describe('ThorVision MCP tools', () => {
  let device: Server;
  let url = '';
  const uploads: Map<string, Buffer>[] = [];
  let jobFailure: { report: Record<string, unknown>; error?: string } | undefined;

  before(async () => {
    const jobs = new Map<string, { digest: string; polls: number }>();
    device = createServer((request, response) => {
      const chunks: Buffer[] = [];
      request.on('data', (chunk) => chunks.push(chunk));
      request.on('end', () => {
        const reply = (status: number, body: unknown) => {
          response.writeHead(status, { 'content-type': 'application/json' });
          response.end(JSON.stringify(body));
        };
        const path = request.url ?? '';
        if (path === '/v1/capabilities') return reply(200, { sdk: '0.1.0', python: '3.13.5', machine: 'aarch64' });
        if (path === '/v1/state') return reply(200, { cameras: {} });
        if (path === '/v1/packages' && request.method === 'POST') {
          const files = untar(Buffer.concat(chunks));
          uploads.push(files);
          return reply(200, { digest: createHash('sha256').update(files.get('manifest.json')!).digest('hex'), manifest: {} });
        }
        const test = /^\/v1\/packages\/([0-9a-f]{64})\/(test|prepare)$/.exec(path);
        if (test) {
          jobs.set('a'.repeat(32), { digest: test[1], polls: 0 });
          return reply(200, { id: 'a'.repeat(32), kind: test[2], digest: test[1], state: 'running' });
        }
        const job = /^\/v1\/jobs\/([0-9a-f]{32})$/.exec(path);
        if (job && jobs.has(job[1])) {
          const entry = jobs.get(job[1])!;
          const done = ++entry.polls > 1;
          if (done && jobFailure) return reply(200, { id: job[1], state: 'failed', ...jobFailure });
          return reply(200, { id: job[1], state: done ? 'succeeded' : 'running', report: done ? { passed: true, digest: entry.digest, frames: 12 } : undefined });
        }
        reply(404, { error: `no route ${path}` });
      });
    });
    await new Promise<void>((resolve) => device.listen(0, '127.0.0.1', resolve));
    url = `http://127.0.0.1:${(device.address() as any).port}`;
  });

  after(() => device.close());

  async function connect(): Promise<{ client: Client; context: ToolContext }> {
    const workspace = mkdtempSync(join(tmpdir(), 'tv-tools-'));
    mkdirSync(join(workspace, 'package'));
    const context: ToolContext = { workspace, camera: 'cam', device: new DeviceClient(url), snapshot: SDK_SNAPSHOT };
    const [clientSide, serverSide] = InMemoryTransport.createLinkedPair();
    await createTools(context).connect(serverSide);
    const client = new Client({ name: 'test', version: '1' });
    await client.connect(clientSide);
    return { client, context };
  }

  const textOf = (result: any) => result.content.map((item: any) => item.text).join('');

  it('exposes exactly the common tool set', async () => {
    const { client } = await connect();
    const names = (await client.listTools()).tools.map((tool) => tool.name).sort();
    assert.deepEqual(names, ['get_control_status', 'get_device_capabilities', 'get_sample_frames', 'get_sdk_contract', 'get_validation_report',
      'open_control_panel', 'prepare_dependencies', 'prepare_extension', 'read_project', 'test_candidate', 'validate_extension', 'write_project']);
    assert.match(textOf(await client.callTool({ name: 'get_sdk_contract', arguments: {} })), /schema digest 34a34e64/);
    assert.match(textOf(await client.callTool({ name: 'get_device_capabilities', arguments: {} })), /aarch64/);
  });

  it('confines files to the extension project', async () => {
    const { client, context } = await connect();
    for (const path of ['../escape.py', '..\\escape.py', 'C:/Windows/x', '/etc/passwd']) {
      const result: any = await client.callTool({ name: 'write_project', arguments: { path, content: 'x' } });
      assert.ok(result.isError, path);
    }
    writeFileSync(join(context.workspace, 'secret.txt'), 'secret');
    try {
      symlinkSync(join(context.workspace, 'secret.txt'), join(context.workspace, 'package', 'link.txt'));
      assert.throws(() => projectPath(context, 'link.txt'), /links are not allowed/);
    } catch (error) {
      if ((error as any).code !== 'EPERM') throw error;  // Creating links needs privileges on Windows.
    }
  });

  it('writes, validates and tests a candidate; the report binds to the exact digest', async () => {
    const { client, context } = await connect();
    for (const name of ['manifest.json', 'processor.py', 'config.schema.json']) {
      const content = name === 'manifest.json'
        ? JSON.stringify({ ...JSON.parse(readFileSync(join(EXAMPLE, name), 'utf8')), content_digest: undefined })
        : readFileSync(join(EXAMPLE, name), 'utf8');
      assert.ok(!(await client.callTool({ name: 'write_project', arguments: { path: name, content } }) as any).isError);
    }
    assert.match(textOf(await client.callTool({ name: 'read_project', arguments: { path: '' } })), /processor.py/);
    const validated = JSON.parse(textOf(await client.callTool({ name: 'validate_extension', arguments: {} })));
    assert.equal(validated.digest, packageDigest(join(context.workspace, 'package')));
    const report = JSON.parse(textOf(await client.callTool({ name: 'test_candidate', arguments: {} })));
    assert.deepEqual([report.state, report.passed, report.digest], ['succeeded', true, validated.digest]);
    assert.equal(uploads.at(-1)!.get('processor.py')!.toString(), readFileSync(join(EXAMPLE, 'processor.py'), 'utf8'));
    const reports = JSON.parse(textOf(await client.callTool({ name: 'get_validation_report', arguments: { digest: validated.digest } })));
    assert.equal(reports[0].digest, validated.digest);
    await client.callTool({ name: 'write_project', arguments: { path: 'processor.py', content: '# changed\n' } });
    assert.notEqual(JSON.parse(textOf(await client.callTool({ name: 'validate_extension', arguments: {} }))).digest, validated.digest);
  });

  it('preserves preparation and test errors in tool responses and saved reports', async () => {
    const { client, context } = await connect();
    cpSync(EXAMPLE, join(context.workspace, 'package'), { recursive: true });
    const checksumError = `model yolo11n checksum mismatch: expected ${'0'.repeat(64)}, received ${'a'.repeat(64)}`;
    try {
      for (const [tool, kind, error] of [
        ['prepare_dependencies', 'prepare', checksumError],
        ['test_candidate', 'test', 'package environment is not prepared'],
      ]) {
        jobFailure = { report: { passed: false, error } };
        const result: any = await client.callTool({ name: tool, arguments: {} });
        assert.equal(result.isError, true);
        const report = JSON.parse(textOf(result));
        assert.equal(report.error, error);
        const saved = JSON.parse(readFileSync(join(context.workspace, 'history', 'reports', `${report.digest}.${kind}.json`), 'utf8'));
        assert.equal(saved.error, error);
        const history = JSON.parse(textOf(await client.callTool({ name: 'get_validation_report', arguments: { digest: report.digest } })));
        assert.equal(history.find((entry: any) => entry.kind === kind).error, error);
      }
      jobFailure = { report: { passed: false }, error: 'preparation job cancelled' };
      const result = JSON.parse(textOf(await client.callTool({ name: 'prepare_dependencies', arguments: {} })));
      assert.equal(result.error, 'preparation job cancelled');
    } finally {
      jobFailure = undefined;
      await client.close();
    }
  });

  it('reports an unreachable device as a tool error instead of throwing', async () => {
    const workspace = mkdtempSync(join(tmpdir(), 'tv-tools-'));
    const context: ToolContext = { workspace, camera: 'cam', device: new DeviceClient('http://127.0.0.1:9'), snapshot: SDK_SNAPSHOT };
    const [clientSide, serverSide] = InMemoryTransport.createLinkedPair();
    await createTools(context).connect(serverSide);
    const client = new Client({ name: 'test', version: '1' });
    await client.connect(clientSide);
    const result: any = await client.callTool({ name: 'get_device_capabilities', arguments: {} });
    assert.ok(result.isError);
    assert.match(textOf(result), /device unavailable/);
  });
});
