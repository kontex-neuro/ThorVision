import assert from 'node:assert/strict';
import { cpSync, existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, writeFileSync } from 'node:fs';
import { createServer } from 'node:http';
import { createHash } from 'node:crypto';
import { tmpdir } from 'node:os';
import { dirname, join, relative, resolve } from 'node:path';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { test } from 'node:test';

const deploy = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const subject = resolve(process.env.THORVISION_TEST_RUNTIME || join(deploy, '../.release-build/package'));
const cli = join(subject, 'bin/thorvision-framelab.mjs');
const temp = () => mkdtempSync(join(tmpdir(), 'thorvision deploy '));
function run(file, args, environment) {
  return new Promise((resolvePromise, reject) => {
    const child = spawn(process.execPath, [file, ...args], { windowsHide: true, ...(environment ? { env: { ...process.env, ...environment } } : {}) });
    let stdout = '', stderr = '';
    child.stdout.on('data', data => { stdout += data; });
    child.stderr.on('data', data => { stderr += data; });
    const timer = setTimeout(() => { child.kill(); reject(new Error('Packaged CLI timed out')); }, 30000);
    child.on('error', reject);
    child.on('close', code => { clearTimeout(timer); resolvePromise({ code, stdout, stderr }); });
  });
}
async function setup(agent, folder, extra = []) {
  return run(cli, ['setup', '--agent', agent, '--workspace', join(folder, 'workspace with spaces'),
    '--device-url', 'http://127.0.0.1:9', '--home', join(folder, 'home'), ...extra]);
}

test('packaged doctor initializes MCP, reads the SDK and serves all WebUI assets without a device', async () => {
  const folder = temp();
  const result = await run(cli, ['doctor', '--workspace', folder, '--device-url', 'http://127.0.0.1:9', '--offline']);
  assert.equal(result.code, 0, result.stderr + result.stdout);
  const report = JSON.parse(result.stdout);
  assert.equal(report.ok, true);
  assert.ok(report.checks.some(check => check.name === 'mcp-tools' && check.count === 14));
  assert.ok(report.checks.some(check => check.name === 'local-webui' && check.ok));
  assert.ok(report.checks.some(check => check.name === 'device-api' && check.skipped));
  assert.doesNotMatch(result.stdout, /#[a-f0-9]{64}|"token"/);
});

test('workspace registration preserves other models and MCP entries, and is idempotent', async () => {
  const folder = temp();
  const workspace = join(folder, 'workspace with spaces');
  mkdirSync(workspace);
  const path = join(workspace, 'opencode.json');
  writeFileSync(path, JSON.stringify({ model: 'fixture/model', instructions: ['文件.md'], mcp: { other: { enabled: false } } }));
  let result = await setup('opencode', folder);
  assert.equal(result.code, 0, result.stderr);
  const config = JSON.parse(readFileSync(path));
  assert.equal(config.model, 'fixture/model');
  assert.deepEqual(config.instructions, ['文件.md']);
  assert.deepEqual(config.mcp.other, { enabled: false });
  assert.ok(config.mcp.framelab.command.includes(workspace));
  assert.equal(config.mcp.framelab.timeout, 900000);
  assert.ok(existsSync(join(folder, 'home/.agents/skills/thorvision-framelab/SKILL.md')));
  const before = readdirSync(workspace).filter(name => name.endsWith('.bak'));
  result = await setup('opencode', folder);
  assert.equal(result.code, 0, result.stderr);
  assert.deepEqual(readdirSync(workspace).filter(name => name.endsWith('.bak')), before);
});

test('host configurations are generated for Gemini, Claude and Codex without account changes', async () => {
  for (const agent of ['gemini', 'claude', 'codex']) {
    const folder = temp();
    const result = await setup(agent, folder);
    assert.equal(result.code, 0, result.stderr);
    const workspace = join(folder, 'workspace with spaces');
    if (agent === 'codex') {
      const text = readFileSync(join(workspace, '.codex/config.toml'), 'utf8');
      assert.match(text, /\[mcp_servers\.framelab\]/);
      assert.match(text, /tool_timeout_sec = 900/);
      assert.ok(existsSync(join(workspace, '.codex/skills/thorvision-framelab/SKILL.md')));
      assert.equal((await setup(agent, folder)).code, 0);
      assert.equal(readFileSync(join(workspace, '.codex/config.toml'), 'utf8'), text);
    } else {
      const file = agent === 'gemini' ? '.gemini/settings.json' : '.mcp.json';
      const config = JSON.parse(readFileSync(join(workspace, file)));
      assert.equal(config.mcpServers.framelab.command, process.execPath);
      assert.ok(config.mcpServers.framelab.args.includes('serve'));
    }
    assert.deepEqual(readdirSync(join(folder, 'home')), ['.agents']);
  }
});

test('a conflicting shared skill prevents partial setup and explicit replacement keeps a backup', async () => {
  const folder = temp();
  const path = join(folder, 'home/.agents/skills/thorvision-framelab/SKILL.md');
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, 'user maintained skill');
  const failed = await setup('opencode', folder);
  assert.notEqual(failed.code, 0);
  assert.equal(existsSync(join(folder, 'workspace with spaces/opencode.json')), false);
  assert.equal(readFileSync(path, 'utf8'), 'user maintained skill');
  const replaced = await setup('opencode', folder, ['--replace-skill']);
  assert.equal(replaced.code, 0, replaced.stderr);
  const backup = readdirSync(dirname(path)).find(name => name.startsWith('SKILL.md.before-framelab-'));
  assert.equal(readFileSync(join(dirname(path), backup), 'utf8'), 'user maintained skill');
});

test('invalid device URLs and existing JSONC fail before changing a workspace', async () => {
  const folder = temp();
  let result = await run(cli, ['setup', '--agent', 'opencode', '--workspace', join(folder, 'new'),
    '--device-url', 'https://example.com/analytics', '--home', join(folder, 'home')]);
  assert.notEqual(result.code, 0);
  assert.equal(existsSync(join(folder, 'new')), false);
  const workspace = join(folder, 'workspace with spaces');
  mkdirSync(workspace);
  writeFileSync(join(workspace, 'opencode.jsonc'), '// existing user configuration');
  result = await setup('opencode', folder);
  assert.notEqual(result.code, 0);
  assert.equal(existsSync(join(workspace, 'opencode.json')), false);
});

test('unmanaged Codex inline and dotted MCP tables require a manual merge without writes', async () => {
  for (const text of ['mcp_servers = { thorvision = { command = "old" } }\n',
    '[mcp_servers]\nother = { command = "old" }\n', '["mcp_servers"."thorvision"]\ncommand = "old"\n']) {
    const folder = temp();
    const path = join(folder, 'workspace with spaces/.codex/config.toml');
    mkdirSync(dirname(path), { recursive: true });
    writeFileSync(path, text);
    const result = await setup('codex', folder);
    assert.notEqual(result.code, 0);
    assert.equal(readFileSync(path, 'utf8'), text);
    assert.equal(existsSync(join(folder, 'home')), false);
  }
});

test('read-only device doctor accepts compatible API and rejects SDK mismatch', async () => {
  let sdk = '0.1.0';
  const requests = [];
  const server = createServer((request, response) => {
    requests.push(`${request.method} ${request.url}`);
    response.setHeader('content-type', 'application/json');
    if (request.url === '/digital-io') { response.writeHead(404); response.end('{"error":"legacy device"}'); return; }
    response.end(JSON.stringify(request.url === '/v1/state' ? { cameras: {} } : { protocol: 1, sdk }));
  });
  await new Promise(done => server.listen(0, '127.0.0.1', done));
  try {
    const args = ['doctor', '--workspace', temp(), '--device-url', `http://127.0.0.1:${server.address().port}`];
    let result = await run(cli, args);
    assert.equal(result.code, 0, result.stdout + result.stderr);
    sdk = '9.9.9';
    result = await run(cli, args);
    assert.notEqual(result.code, 0);
    assert.match(result.stdout, /SDK\/protocol mismatch/);
    assert.deepEqual(requests.sort(), ['GET /digital-io', 'GET /digital-io', 'GET /v1/capabilities', 'GET /v1/capabilities', 'GET /v1/state', 'GET /v1/state']);
  } finally { await new Promise(done => server.close(done)); }
});

test('altered shipped UI is rejected before the MCP server starts', async () => {
  const folder = temp();
  const copied = join(folder, 'runtime');
  cpSync(subject, copied, { recursive: true, filter: path => !relative(subject, path).split(/[\\/]/).includes('node_modules') });
  writeFileSync(join(copied, 'control-ui/app.js'), 'tampered');
  const result = await run(join(copied, 'bin/thorvision-mcp.mjs'), ['serve', '--workspace', folder, '--device-url', 'http://127.0.0.1:9']);
  assert.notEqual(result.code, 0);
  assert.match(result.stderr, /Release file is missing or changed: control-ui\/app.js/);
});

test('installer refuses an archive hash mismatch before creating the install prefix', async () => {
  const folder = temp();
  const artifact = join(folder, 'release.tgz');
  writeFileSync(artifact, 'not the reviewed release');
  const prefix = join(folder, 'new-runtime');
  const result = await run(join(deploy, 'install.mjs'), ['--artifact', artifact, '--sha256', '0'.repeat(64), '--prefix', prefix]);
  assert.notEqual(result.code, 0);
  assert.match(result.stderr, /SHA-256 mismatch/);
  assert.equal(existsSync(prefix), false);
});

test('installer rechecks an existing runtime instead of trusting its receipt alone', async () => {
  const prefix = temp();
  const digest = 'a'.repeat(64);
  writeFileSync(join(prefix, 'thorvision-install.json'), JSON.stringify({ artifactSha256: digest }));
  const result = await run(join(deploy, 'install.mjs'), ['--artifact', join(prefix, 'absent.tgz'), '--sha256', digest, '--prefix', prefix]);
  assert.notEqual(result.code, 0);
  assert.match(result.stderr, /Runtime file verification failed/);
  assert.doesNotMatch(result.stdout, /alreadyInstalled/);
});

test('canonical and legacy CLIs verify the same FrameLab release', async () => {
  const current = await run(cli, ['verify']);
  const previous = await run(join(subject, 'bin/thorvision-mcp.mjs'), ['verify']);
  assert.equal(current.code, 0, current.stderr);
  assert.equal(previous.code, 0, previous.stderr);
  assert.deepEqual(JSON.parse(previous.stdout), JSON.parse(current.stdout));
  assert.equal(JSON.parse(current.stdout).version, '0.2.0');
});

test('legacy managed JSON registrations migrate to exactly one FrameLab server', async () => {
  for (const agent of ['opencode', 'gemini', 'claude']) {
    const folder = temp();
    const workspace = join(folder, 'workspace with spaces');
    const oldRuntime = join(folder, 'previous-runtime');
    const args = [join(oldRuntime, 'bin/thorvision-mcp.mjs'), 'serve', '--workspace', workspace, '--device-url', 'http://127.0.0.1:9'];
    const entry = agent === 'opencode'
      ? { type: 'local', command: [process.execPath, ...args], enabled: true, timeout: 900000 }
      : { command: process.execPath, args, ...(agent === 'gemini' ? { timeout: 900000 } : {}) };
    const file = join(workspace, agent === 'opencode' ? 'opencode.json' : agent === 'gemini' ? '.gemini/settings.json' : '.mcp.json');
    const key = agent === 'opencode' ? 'mcp' : 'mcpServers';
    mkdirSync(dirname(file), { recursive: true });
    writeFileSync(file, JSON.stringify({ model: 'keep-me', [key]: { other: { command: 'keep-me' }, thorvision: entry } }));
    mkdirSync(join(workspace, '.thorvision'), { recursive: true });
    writeFileSync(join(workspace, '.thorvision/workspace.json'), JSON.stringify({ schema: 'thorvision.workspace/1', runtime: oldRuntime,
      workspace, camera: '', deviceUrl: 'http://127.0.0.1:9', customSetting: 'preserve-me' }));
    const result = await setup(agent, folder);
    assert.equal(result.code, 0, result.stderr);
    const value = JSON.parse(readFileSync(file));
    assert.equal(value.model, 'keep-me');
    assert.deepEqual(Object.keys(value[key]).sort(), ['framelab', 'other']);
    assert.ok(readdirSync(dirname(file)).some(name => name.endsWith('.bak')));
    assert.equal(JSON.parse(readFileSync(join(workspace, '.thorvision/workspace.json'))).customSetting, 'preserve-me');
    assert.ok(existsSync(join(folder, 'home/.agents/skills/thorvision-analytics/SKILL.md')));
    assert.match(readFileSync(join(folder, 'home/.agents/skills/thorvision-analytics/SKILL.md'), 'utf8'), /legacy entry point/);
  }
});

test('unmanaged and edited owned JSON entries are never overwritten', async () => {
  for (const key of ['thorvision', 'framelab']) {
    const folder = temp();
    const file = join(folder, 'workspace with spaces/opencode.json');
    mkdirSync(dirname(file), { recursive: true });
    const original = JSON.stringify({ mcp: { [key]: { type: 'local', command: ['custom-tool'], enabled: true } } });
    writeFileSync(file, original);
    const result = await setup('opencode', folder);
    assert.notEqual(result.code, 0);
    assert.match(result.stderr, /Unmanaged or changed/);
    assert.equal(readFileSync(file, 'utf8'), original);
    assert.equal(existsSync(join(folder, 'home')), false);
  }
  const folder = temp();
  assert.equal((await setup('opencode', folder)).code, 0);
  const file = join(folder, 'workspace with spaces/opencode.json');
  const value = JSON.parse(readFileSync(file));
  value.mcp.framelab.timeout = 1234;
  const edited = JSON.stringify(value);
  writeFileSync(file, edited);
  const result = await setup('opencode', folder);
  assert.notEqual(result.code, 0);
  assert.equal(readFileSync(file, 'utf8'), edited);
});

test('legacy managed Codex block migrates while unrelated MCP tables remain intact', async () => {
  const folder = temp();
  const file = join(folder, 'workspace with spaces/.codex/config.toml');
  mkdirSync(dirname(file), { recursive: true });
  const unrelated = 'model = "keep-model"\n[mcp_servers.other]\ncommand = "keep-command"\n';
  writeFileSync(file, `${unrelated}\n# BEGIN THORVISION MCP (managed)\n[mcp_servers.thorvision]\ncommand = "old"\n# END THORVISION MCP (managed)\n`);
  const result = await setup('codex', folder);
  assert.equal(result.code, 0, result.stderr);
  const text = readFileSync(file, 'utf8');
  assert.ok(text.startsWith(unrelated));
  assert.match(text, /\[mcp_servers\.framelab\]/);
  assert.doesNotMatch(text, /\[mcp_servers\.thorvision\]/);
  assert.equal((text.match(/BEGIN THORVISION FRAMELAB/g) || []).length, 1);
});

test('incomplete or duplicate managed TOML markers stop without writes', async () => {
  for (const text of ['# BEGIN THORVISION MCP (managed)\n[mcp_servers.thorvision]\n',
    '# BEGIN THORVISION MCP (managed)\n# BEGIN THORVISION MCP (managed)\n# END THORVISION MCP (managed)\n',
    'instructions = """\n# BEGIN THORVISION MCP (managed)\nkeep these instructions\n# END THORVISION MCP (managed)\n"""\n']) {
    const folder = temp();
    const file = join(folder, 'workspace with spaces/.codex/config.toml');
    mkdirSync(dirname(file), { recursive: true });
    writeFileSync(file, text);
    const result = await setup('codex', folder);
    assert.notEqual(result.code, 0);
    assert.equal(readFileSync(file, 'utf8'), text);
    assert.equal(existsSync(join(folder, 'home')), false);
  }
});

test('FrameLab URL environment takes precedence while the legacy variable remains supported', async () => {
  for (const canonical of [true, false]) {
    const folder = temp();
    const workspace = join(folder, 'workspace');
    const result = await run(cli, ['setup', '--agent', 'opencode', '--workspace', workspace, '--home', join(folder, 'home')],
      { THORVISION_FRAMELAB_URL: canonical ? 'http://127.0.0.1:12001/framelab' : '', THORVISION_EXTENSIONS_URL: 'http://127.0.0.1:12002/analytics' });
    assert.equal(result.code, 0, result.stderr);
    const saved = JSON.parse(readFileSync(join(workspace, '.thorvision/workspace.json')));
    assert.equal(saved.deviceUrl, canonical ? 'http://127.0.0.1:12001/framelab' : 'http://127.0.0.1:12002/analytics');
  }
});

test('known managed skill contents upgrade with a backup without an unmanaged replacement flag', async () => {
  const folder = temp();
  const workspace = join(folder, 'workspace with spaces');
  const skill = join(folder, 'home/.agents/skills/thorvision-framelab/SKILL.md');
  const prior = 'previous managed FrameLab skill';
  mkdirSync(dirname(skill), { recursive: true });
  writeFileSync(skill, prior);
  mkdirSync(join(workspace, '.thorvision'), { recursive: true });
  writeFileSync(join(workspace, '.thorvision/workspace.json'), JSON.stringify({ schema: 'thorvision.workspace/1',
    managedSkills: { [skill]: createHash('sha256').update(prior).digest('hex') } }));
  const result = await setup('opencode', folder);
  assert.equal(result.code, 0, result.stderr);
  assert.notEqual(readFileSync(skill, 'utf8'), prior);
  const backup = readdirSync(dirname(skill)).find(name => name.endsWith('.bak'));
  assert.equal(readFileSync(join(dirname(skill), backup), 'utf8'), prior);
});

test('migrating one host retains ownership evidence for another host in the same workspace', async () => {
  const folder = temp();
  const workspace = join(folder, 'workspace with spaces');
  const prior = join(folder, 'old-runtime');
  const args = [join(prior, 'bin/thorvision-mcp.mjs'), 'serve', '--workspace', workspace, '--device-url', 'http://127.0.0.1:9'];
  mkdirSync(join(workspace, '.thorvision'), { recursive: true });
  writeFileSync(join(workspace, '.thorvision/workspace.json'), JSON.stringify({ schema: 'thorvision.workspace/1',
    workspace, runtime: prior, deviceUrl: 'http://127.0.0.1:9', camera: '' }));
  writeFileSync(join(workspace, 'opencode.json'), JSON.stringify({ mcp: { thorvision: {
    type: 'local', command: [process.execPath, ...args], enabled: true, timeout: 900000 } } }));
  mkdirSync(join(workspace, '.gemini'), { recursive: true });
  writeFileSync(join(workspace, '.gemini/settings.json'), JSON.stringify({ mcpServers: { thorvision: {
    command: process.execPath, args, timeout: 900000 } } }));
  assert.equal((await setup('opencode', folder)).code, 0);
  const result = await setup('gemini', folder);
  assert.equal(result.code, 0, result.stderr);
  assert.deepEqual(Object.keys(JSON.parse(readFileSync(join(workspace, '.gemini/settings.json'))).mcpServers), ['framelab']);
});
