#!/usr/bin/env node
import { createHash } from 'node:crypto';
import { existsSync, mkdirSync, readFileSync, writeFileSync, copyFileSync } from 'node:fs';
import { homedir } from 'node:os';
import { dirname, join, relative, resolve, sep } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

export const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const json = path => JSON.parse(readFileSync(path, 'utf8'));
const pretty = value => JSON.stringify(value, null, 2) + '\n';
const sha256 = bytes => createHash('sha256').update(bytes).digest('hex');
const object = value => value && typeof value === 'object' && !Array.isArray(value);

function options(args) {
  const result = {};
  const allowed = new Set(['workspace', 'device-url', 'camera', 'home', 'agent', 'offline', 'json', 'replace-skill']);
  for (let index = 0; index < args.length; index++) {
    const name = args[index];
    if (!name.startsWith('--')) throw new Error(`Unexpected argument: ${name}`);
    if (!allowed.has(name.slice(2))) throw new Error(`Unknown option: ${name}`);
    if (['--offline', '--json', '--replace-skill'].includes(name)) result[name.slice(2)] = true;
    else {
      if (!args[index + 1] || args[index + 1].startsWith('--')) throw new Error(`Missing value for ${name}`);
      result[name.slice(2)] = args[++index];
    }
  }
  return result;
}

export function verifyRelease() {
  const release = json(join(root, 'release.json'));
  for (const [path, expected] of Object.entries(release.files)) {
    const target = resolve(root, path);
    if (!target.startsWith(root + sep) || !existsSync(target) || sha256(readFileSync(target)) !== expected)
      throw new Error(`Release file is missing or changed: ${path}`);
  }
  return release;
}

async function context(flags) {
  const workspace = resolve(flags.workspace || process.env.THORVISION_WORKSPACE || process.cwd());
  const path = join(workspace, '.thorvision', 'workspace.json');
  const saved = existsSync(path) ? json(path) : {};
  const deviceUrl = flags['device-url'] || process.env.THORVISION_FRAMELAB_URL || process.env.THORVISION_EXTENSIONS_URL || saved.deviceUrl;
  if (!deviceUrl) throw new Error('Provide --device-url http://DEVICE_IP:8000/framelab or run setup in this workspace.');
  const { validateDeviceUrl } = await import(pathToFileURL(join(root, 'dist/device-url.js')));
  const normalized = validateDeviceUrl(deviceUrl).href.replace(/\/$/, '');
  return { workspace, deviceUrl: normalized, camera: flags.camera || process.env.THORVISION_CAMERA || saved.camera || '' };
}

// Keep unrelated host configuration and back up each file we actually change.
function writeSaved(path, text) {
  if (existsSync(path) && readFileSync(path, 'utf8') === text) return;
  mkdirSync(dirname(path), { recursive: true });
  if (existsSync(path)) copyFileSync(path, `${path}.before-framelab-${Date.now()}-${process.hrtime.bigint()}.bak`);
  writeFileSync(path, text, 'utf8');
}

const entryHash = entry => sha256(JSON.stringify(entry));

function ownedAt(entry, saved, workspace, selected) {
  if (saved.schema !== 'thorvision.workspace/1' || typeof saved.runtime !== 'string' || saved.workspace !== workspace || !object(entry)) return false;
  const args = selected === 'opencode' ? entry.command?.slice(1) : entry.args;
  if (!Array.isArray(args) || !['thorvision-mcp.mjs', 'thorvision-framelab.mjs'].some(name => args[0] === join(saved.runtime, 'bin', name)) || args[1] !== 'serve') return false;
  const expectedArgs = [args[0], 'serve', '--workspace', workspace, '--device-url', saved.deviceUrl];
  if (saved.camera) expectedArgs.push('--camera', saved.camera);
  if (JSON.stringify(args) !== JSON.stringify(expectedArgs)) return false;
  const command = selected === 'opencode' ? entry.command[0] : entry.command;
  if (typeof command !== 'string') return false;
  const expected = selected === 'opencode'
    ? { type: 'local', command: [command, ...expectedArgs], enabled: true, timeout: 900000 }
    : { command, args: expectedArgs, ...(selected === 'gemini' ? { timeout: 900000 } : {}) };
  return Object.keys(entry).length === Object.keys(expected).length
    && Object.keys(expected).every(key => JSON.stringify(entry[key]) === JSON.stringify(expected[key]));
}

function runtimeContexts(saved) {
  return [saved, ...(Array.isArray(saved.runtimeHistory) ? saved.runtimeHistory.slice(0, 32) : [])]
    .filter(value => object(value) && value.schema === 'thorvision.workspace/1' && typeof value.runtime === 'string');
}

function legacyOwned(entry, saved, workspace, selected) {
  return runtimeContexts(saved).some(value => ownedAt(entry, value, workspace, selected));
}

function configuredJson(path, key, entry, saved, workspace, selected) {
  const value = existsSync(path) ? json(path) : {};
  if (!object(value) || (value[key] !== undefined && !object(value[key]))) throw new Error(`Invalid ${key} object in ${path}`);
  const servers = { ...value[key] };
  for (const name of ['thorvision', 'framelab']) {
    if (servers[name] === undefined) continue;
    const record = saved.managedMcp?.[selected];
    const recorded = record?.key === name && record.entrySha256 === entryHash(servers[name]);
    if (!recorded && (record?.key === name || !legacyOwned(servers[name], saved, workspace, selected)))
      throw new Error(`Unmanaged or changed ${name} MCP entry in ${path}; review and merge it before FrameLab setup. No configuration was changed.`);
    delete servers[name];
  }
  value[key] = { ...servers, framelab: entry };
  return value;
}

function codexConfig(path, command, args) {
  const begin = '# BEGIN THORVISION FRAMELAB MCP (managed)';
  const end = '# END THORVISION FRAMELAB MCP (managed)';
  const original = existsSync(path) ? readFileSync(path, 'utf8') : '';
  // Do not interpret apparent comment markers inside multiline TOML strings.
  // A full TOML rewriter is intentionally outside this small installer.
  if ((original.includes('"""') || original.includes("'''"))
      && /^\s*# (?:BEGIN|END) THORVISION(?: FRAMELAB)? MCP \(managed\)\s*$/m.test(original))
    throw new Error('Managed MCP markers coexist with multiline TOML strings; review and merge this configuration manually.');
  let retained = original;
  for (const prefix of ['THORVISION', 'THORVISION FRAMELAB']) {
    const startMarker = `# BEGIN ${prefix} MCP (managed)`;
    const endMarker = `# END ${prefix} MCP (managed)`;
    const lines = retained.split(/\r?\n/);
    const starts = lines.flatMap((line, index) => line.trim() === startMarker ? [index] : []);
    const ends = lines.flatMap((line, index) => line.trim() === endMarker ? [index] : []);
    if (!starts.length && !ends.length) continue;
    if (starts.length !== 1 || ends.length !== 1 || ends[0] < starts[0])
      throw new Error('Incomplete or duplicate FrameLab/ThorVision managed block; review the Codex configuration first.');
    lines.splice(starts[0], ends[0] - starts[0] + 1);
    retained = lines.join('\n');
  }
  // Preserve unrelated simple MCP tables verbatim. Ambiguous inline/root/dotted
  // declarations require review instead of attempting a partial TOML rewrite.
  const key = `(?:mcp_servers|"mcp_servers"|'mcp_servers')`;
  if (new RegExp(`^\\s*${key}\\s*[.=]`, 'm').test(retained)
      || new RegExp(`^\\s*\\[\\s*${key}\\s*\\]`, 'm').test(retained)
      || new RegExp(`^\\s*\\[\\s*${key}\\s*\\.\\s*(?:framelab|thorvision|"framelab"|"thorvision"|'framelab'|'thorvision')\\s*(?:\\.|\\])`, 'm').test(retained))
    throw new Error('Unmanaged or ambiguous Codex MCP settings require a manual merge before FrameLab setup.');
  return retained.trimEnd() + `\n\n${begin}\n[mcp_servers.framelab]\ncommand = ${JSON.stringify(command)}\nargs = ${JSON.stringify(args)}\ntool_timeout_sec = 900\n${end}\n`;
}

export async function setup(flags) {
  const release = verifyRelease();
  const selected = flags.agent;
  if (!['opencode', 'gemini', 'claude', 'codex'].includes(selected)) throw new Error('Choose --agent opencode, gemini, claude or codex.');
  const current = await context(flags);
  const workspacePath = join(current.workspace, '.thorvision', 'workspace.json');
  const saved = existsSync(workspacePath) ? json(workspacePath) : {};
  if (!object(saved) || (saved.schema !== undefined && saved.schema !== 'thorvision.workspace/1')) throw new Error('Unsupported ThorVision workspace record; review before setup.');
  const args = [join(root, 'bin', 'thorvision-framelab.mjs'), 'serve', '--workspace', current.workspace, '--device-url', current.deviceUrl];
  if (current.camera) args.push('--camera', current.camera);
  const hostPaths = {
    opencode: join(current.workspace, 'opencode.json'),
    gemini: join(current.workspace, '.gemini', 'settings.json'),
    claude: join(current.workspace, '.mcp.json'),
    codex: join(current.workspace, '.codex', 'config.toml'),
  };
  const hostPath = hostPaths[selected];
  if (selected === 'opencode' && existsSync(join(current.workspace, 'opencode.jsonc')))
    throw new Error('opencode.jsonc exists; merge the FrameLab entry there rather than creating a second configuration.');
  let hostText;
  let managedEntry;
  if (selected === 'codex') hostText = codexConfig(hostPath, process.execPath, args);
  else {
    const entry = selected === 'opencode'
      ? { type: 'local', command: [process.execPath, ...args], enabled: true, timeout: 900000 }
      : { command: process.execPath, args, ...(selected === 'gemini' ? { timeout: 900000 } : {}) };
    managedEntry = entry;
    const value = configuredJson(hostPath, selected === 'opencode' ? 'mcp' : 'mcpServers', entry, saved, current.workspace, selected);
    if (selected === 'opencode') value.$schema ??= 'https://opencode.ai/config.json';
    hostText = pretty(value);
  }
  const home = resolve(flags.home || homedir());
  const skillRoots = [join(home, '.agents', 'skills')];
  if (selected === 'codex') skillRoots.push(join(current.workspace, '.codex', 'skills'));
  const managedMcp = { ...saved.managedMcp, [selected]: managedEntry ? { key: 'framelab', entrySha256: entryHash(managedEntry) } : { key: 'framelab', format: 'managed-toml-block' } };
  const managedSkills = { ...saved.managedSkills };
  const priorSkillFiles = [];
  for (const previous of runtimeContexts(saved)) {
    const priorRelease = join(previous.runtime, 'release.json');
    if (existsSync(priorRelease)) {
      const prior = json(priorRelease);
      if (prior.schema === 'thorvision.mcp-release/1' && prior.filesSha256 === previous.releaseFilesSha256)
        priorSkillFiles.push(prior.files || {});
    }
  }
  const writes = [[hostPath, hostText]];
  for (const skillRoot of skillRoots) {
    for (const file of Object.keys(release.files).filter(name => /^skills\/thorvision-(?:framelab|analytics)\//.test(name))) {
      const path = join(skillRoot, relative('skills', file));
      const content = readFileSync(join(root, file), 'utf8');
      if (existsSync(path) && readFileSync(path, 'utf8') !== content) {
        const digest = sha256(readFileSync(path));
        const owned = saved.managedSkills?.[path] === digest || priorSkillFiles.some(files => files[file] === digest);
        if (!owned && !flags['replace-skill'])
          throw new Error(`A different skill exists at ${path}; review it and use --replace-skill to back up and replace it.`);
      }
      managedSkills[path] = sha256(Buffer.from(content));
      writes.push([path, content]);
    }
  }
  const history = new Map(runtimeContexts(saved).map(previous => {
    const entry = { schema: 'thorvision.workspace/1', runtime: previous.runtime, workspace: previous.workspace,
      deviceUrl: previous.deviceUrl, camera: previous.camera || '', releaseFilesSha256: previous.releaseFilesSha256 };
    return [JSON.stringify(entry), entry];
  }));
  // Do not add the current unchanged record on each idempotent setup.
  for (const [key, previous] of history) {
    if (previous.runtime === root && previous.deviceUrl === current.deviceUrl && previous.camera === current.camera && previous.workspace === current.workspace) history.delete(key);
  }
  writes.push([workspacePath, pretty({
    ...saved, schema: 'thorvision.workspace/1', ...current, runtimeVersion: release.version, managedMcp, managedSkills,
    runtimeHistory: [...history.values()].slice(0, 32),
    runtime: root, releaseFilesSha256: release.filesSha256,
  })]);
  // No filesystem writes until all configuration/skill conflicts have been checked.
  for (const [path, text] of writes) writeSaved(path, text);
  return { configured: true, agent: selected, config: hostPath, ...current, skillRoots,
    next: 'Restart the chosen agent, trust this workspace if requested, sign in/select a model, then ask it to open_control_panel. Device services are developer-predeployed.' };
}

export async function doctor(flags) {
  const checks = [];
  let transport;
  let client;
  try {
    const release = verifyRelease();
    checks.push({ name: 'release-files', ok: true, version: release.version, sourceDirty: release.source.dirty });
    const current = await context(flags);
    const { Client } = await import('@modelcontextprotocol/sdk/client/index.js');
    const { StdioClientTransport } = await import('@modelcontextprotocol/sdk/client/stdio.js');
    transport = new StdioClientTransport({ command: process.execPath, args: [fileURLToPath(import.meta.url), 'serve',
      '--workspace', current.workspace, '--device-url', current.deviceUrl], stderr: 'pipe' });
    // Child errors are summarized without exposing session-local control tokens.
    transport.stderr?.on('data', () => {});
    client = new Client({ name: 'thorvision-framelab-doctor', version: release.version });
    await client.connect(transport, { timeout: 15000 });
    const tools = await client.listTools();
    for (const name of ['get_sdk_contract', 'prepare_extension', 'open_control_panel', 'get_control_status']) {
      if (!tools.tools.some(tool => tool.name === name)) throw new Error(`Required MCP tool missing: ${name}`);
    }
    checks.push({ name: 'mcp-tools', ok: true, count: tools.tools.length });
    const contract = await client.callTool({ name: 'get_sdk_contract', arguments: {} });
    if (contract.isError) throw new Error('SDK reference could not be read.');
    checks.push({ name: 'sdk-contract', ok: true, sdk: release.sdk.sdk_version });
    const panel = await client.callTool({ name: 'open_control_panel', arguments: {} });
    if (panel.isError) throw new Error('Local control panel did not start.');
    const connection = JSON.parse(panel.content.find(item => item.type === 'text').text);
    for (const path of ['/', '/app.js', '/style.css']) {
      const response = await fetch(new URL(path, connection.url), { signal: AbortSignal.timeout(5000) });
      if (!response.ok || !(await response.text()).length) throw new Error(`Control UI asset unavailable: ${path}`);
    }
    checks.push({ name: 'local-webui', ok: true });
    if (!flags.offline) {
      const result = await client.callTool({ name: 'get_device_capabilities', arguments: {} }, undefined, { timeout: 20000 });
      const body = result.content.find(item => item.type === 'text')?.text || '';
      if (result.isError) throw new Error(body);
      const capabilities = JSON.parse(body).capabilities;
      if (!object(capabilities)) throw new Error('The MCP capability response has no device capabilities object.');
      if (capabilities.sdk !== release.sdk.sdk_version || capabilities.protocol !== release.sdk.protocol)
        throw new Error(`Device SDK/protocol mismatch: ${capabilities.sdk}/${capabilities.protocol}`);
      checks.push({ name: 'device-api', ok: true, sdk: capabilities.sdk, protocol: capabilities.protocol });
    } else checks.push({ name: 'device-api', skipped: true, reason: '--offline: no device network request' });
    return { ok: true, checks };
  } catch (error) {
    return { ok: false, checks, error: error.message };
  } finally {
    await client?.close().catch(() => {});
    await transport?.close().catch(() => {});
  }
}

export async function main() {
  if (Number(process.versions.node.split('.')[0]) < 22) throw new Error('Node.js 22 or newer is required.');
  const [command = 'help', ...args] = process.argv.slice(2);
  const flags = options(args);
  if (command === 'help' || command === '--help') {
    console.log('ThorVision FrameLab: setup --agent NAME --workspace DIR --device-url URL [--replace-skill]\nserve [--workspace DIR --device-url URL]\ndoctor --workspace DIR [--offline] [--json]\nverify');
    return;
  }
  if (command === 'verify') {
    const release = verifyRelease();
    console.log(pretty({ ok: true, version: release.version, filesSha256: release.filesSha256 }));
    return;
  }
  if (command === 'serve') {
    verifyRelease();
    const current = await context(flags);
    process.env.THORVISION_WORKSPACE = current.workspace;
    process.env.THORVISION_FRAMELAB_URL = current.deviceUrl;
    process.env.THORVISION_EXTENSIONS_URL = current.deviceUrl;
    process.env.THORVISION_CAMERA = current.camera;
    process.env.THORVISION_SDK_SNAPSHOT = join(root, 'sdk-snapshot');
    const { startStdioServer } = await import(pathToFileURL(join(root, 'dist/tools/server.js')));
    await startStdioServer();
    return;
  }
  const result = command === 'setup' ? await setup(flags) : command === 'doctor' ? await doctor(flags) : null;
  if (!result) throw new Error(`Unknown command: ${command}`);
  console.log(pretty(result));
  if (result.ok === false) process.exitCode = 1;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  main().catch(error => { console.error(error.message); process.exitCode = 1; });
}
