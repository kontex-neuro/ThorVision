// ThorVision tools for every agent, served over MCP stdio by the provider runtime's MCP client.
//
// Environment: THORVISION_WORKSPACE (extension project), THORVISION_EXTENSIONS_URL (device API),
// THORVISION_CAMERA, THORVISION_SDK_SNAPSHOT. Agents edit only <workspace>/package; activation
// is not a tool: only the client's Apply can activate a tested digest.
import { appendFileSync, existsSync, lstatSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from 'node:fs';
import { dirname, isAbsolute, join, normalize, relative, resolve, sep } from 'node:path';
import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import { z } from 'zod';
import { DeviceClient, DeviceError } from '../device.js';
import { contentDigest, packageDigest, packTarGz, packageFiles, seal, validateManifest } from '../package.js';
import { ControlWorkbench } from '../control-server.js';

const MAX_FILE_BYTES = 1 << 20;
const MAX_FILES = 200;

export interface ToolContext {
  workspace: string;
  camera: string;
  device: DeviceClient;
  snapshot: string;
}

type Content = { type: 'text'; text: string } | { type: 'image'; data: string; mimeType: string };
type Result = { content: Content[]; isError?: boolean };
type Progress = (message: string) => void;

const text = (value: unknown, isError = false): Result => ({
  content: [{ type: 'text', text: typeof value === 'string' ? value : JSON.stringify(value, null, 2) }],
  ...(isError ? { isError } : {}),
});

/** Resolves a workspace-relative path inside <workspace>/package, refusing escapes and links. */
export function projectPath(context: ToolContext, path: string): string {
  const root = resolve(context.workspace, 'package');
  if (isAbsolute(path) || /^[a-zA-Z]:/.test(path)) throw new Error('paths are relative to the extension project');
  const target = resolve(root, normalize(path || '.'));
  if (target !== root && !target.startsWith(root + sep)) throw new Error(`${path}: outside the extension project`);
  let current = root;
  for (const part of relative(root, target).split(sep).filter(Boolean)) {
    current = join(current, part);
    if (existsSync(current) && lstatSync(current).isSymbolicLink()) throw new Error(`${path}: links are not allowed`);
  }
  return target;
}

function record(context: ToolContext, entry: Record<string, unknown>): void {
  const history = join(context.workspace, 'history');
  mkdirSync(history, { recursive: true });
  appendFileSync(join(history, 'log.jsonl'), JSON.stringify({ time: new Date().toISOString(), ...entry }) + '\n');
}

function saveReport(context: ToolContext, digest: string, report: unknown): void {
  const reports = join(context.workspace, 'history', 'reports');
  mkdirSync(reports, { recursive: true });
  writeFileSync(join(reports, `${digest}.json`), JSON.stringify(report, null, 2));
}

function sdkVersion(context: ToolContext): string {
  return JSON.parse(readFileSync(join(context.snapshot, 'SNAPSHOT.json'), 'utf8')).sdk_version;
}

export function sdkContract(context: ToolContext): string {
  const snapshot = JSON.parse(readFileSync(join(context.snapshot, 'SNAPSHOT.json'), 'utf8'));
  const read = (path: string) => readFileSync(join(context.snapshot, path), 'utf8');
  return [
    `# ThorVision analytics SDK ${snapshot.sdk_version} (schema digest ${snapshot.schema_digest})`,
    'Write a Python package in the extension project (read_project / write_project). It needs manifest.json',
    '(schema thorvision.package/1, name, version, sdk requirement such as ">=0.1,<0.2", entrypoint "processor:Processor")',
    'and the processor module. The class implements setup(context, config), process(frame) -> FrameResult and close().',
    'process() runs on the device in an isolated worker: no network, no device or file access beyond its package',
    'and a private /tmp, 2 s per call, 60 s setup, about 5 FPS on frames fitted within 640x480. Return drawings in',
    'source-image pixels (use frame.transform.box_to_source for processing-image boxes). Check device capabilities.',
    'Declare external dependencies in manifest.dependencies and finish edits with prepare_extension.',
    'This single preparation stage validates, automatically reuses compatible dependencies and runs the device test.',
    'Users describe the desired behavior; do not require them to request reuse, validation, testing or device connection.',
    'When needed, the device resolves/downloads wheels through a temporary PC proxy,',
    'then installs verified files offline in its own environment. V1 supports PyPI wheels only: no source builds, URLs, pip options',
    'or environment markers in requirements.',
    'If capabilities.preparation.environment_reuse.supported is true, preparation automatically reuses',
    'a compatible exact dependency/model lock for callback-only revisions, without duplicating the environment.',
    'Every revised callback still needs a fresh test; a cached environment does not make its test pass.',
    'Model declarations need name, path beneath models/, source HTTPS URL,',
    'sha256 and license. Official Ultralytics GitHub release assets are supported model sources. Read weights from',
    'context["package_dir"] / the declared relative model path. Never download or install inside setup/process.',
    'A checksum mismatch reports the downloaded checksum; confirm the intended official asset before correcting its declaration.',
    'Preparation may fail on size, native-library, ABI or resource limits; report the actual failure without fake detections.',
    'Call prepare_extension after the final edit, then read its report. Do not run redundant tests after it passes.',
    'You cannot activate a package: tell the',
    'user the tested digest, and they review the preview and press Apply in ThorVision.',
    '',
    '## README.md',
    read('README.md'),
    '## thorvision_sdk/contract.py',
    read('thorvision_sdk/contract.py'),
    '## thorvision_sdk/validate.py (limits)',
    read('thorvision_sdk/validate.py'),
    '## Example: examples/moving_green_box',
    read('examples/moving_green_box/manifest.json'),
    read('examples/moving_green_box/processor.py'),
    '## Reference tracker: thorvision_sdk/tracking.py',
    read('thorvision_sdk/tracking.py'),
  ].join('\n');
}

export async function testCandidate(context: ToolContext, kind: 'test' | 'prepare' = 'test',
  progress: Progress = () => {}, signal?: AbortSignal): Promise<Result> {
  const directory = projectPath(context, '');
  const { manifest, problems } = validateManifest(directory, sdkVersion(context));
  if (problems.length) return text({ passed: false, problems }, true);
  const digest = seal(directory);
  const uploaded = await context.device.upload(packTarGz(directory));
  if (uploaded.digest !== digest) return text({ passed: false, problems: [`device digest ${uploaded.digest} differs from local ${digest}`] }, true);
  let job;
  try {
    job = await context.device.job(kind, digest, kind === 'prepare' ? 900000 : 300000,
      kind === 'prepare' && (manifest.dependencies || manifest.models?.length) ? { download: 'proxy' } : undefined,
      progress, signal);
  } catch (error) {
    const report = { kind, digest, state: 'failed', passed: false, error: (error as Error).message };
    saveReport(context, `${digest}.${kind}`, report);
    return text(report, true);
  }
  // Normal job failures carry their diagnostics inside report; transport failures may use error.
  const error = job.error ?? job.report?.error;
  const report = { kind, digest, state: job.state, ...(job.report ?? {}), ...(error !== undefined ? { error } : {}) };
  saveReport(context, `${digest}.${kind}`, report);
  record(context, { event: kind, digest, state: job.state });
  return text(report, job.state !== 'succeeded' || report.passed !== true);
}

/** One preparation action owns validation, environment reuse and the exact-candidate test. */
export async function prepareExtension(context: ToolContext, progress: Progress = () => {}, signal?: AbortSignal): Promise<Result> {
  const existing = preparations.get(context);
  if (existing) return existing;
  const pending = runPreparation(context, progress, signal);
  preparations.set(context, pending);
  try { return await pending; } finally { preparations.delete(context); }
}
const preparations = new WeakMap<ToolContext, Promise<Result>>();

async function runPreparation(context: ToolContext, progress: Progress, signal?: AbortSignal): Promise<Result> {
  signal?.throwIfAborted();
  progress('Validating package and preparing dependencies…');
  const prepared = await testCandidate(context, 'prepare', progress, signal);
  if (prepared.isError) return prepared;
  signal?.throwIfAborted();
  progress('Testing the prepared candidate on the device…');
  const tested = await testCandidate(context, 'test', progress, signal);
  if (tested.isError) return tested;
  const preparation = JSON.parse((prepared.content[0] as { text: string }).text);
  const test = JSON.parse((tested.content[0] as { text: string }).text);
  if (preparation.digest !== test.digest || seal(projectPath(context, '')) !== test.digest)
    return text({ ready: false, passed: false, error: 'Project changed during preparation; prepare the current revision again.' }, true);
  const report = { ...test, kind: 'ready', ready: true, preparation, tested_at: Date.now() };
  saveReport(context, `${test.digest}.ready`, report);
  progress('Ready to apply. Validation and device testing passed.');
  return text(report);
}

export function createServer(context: ToolContext): McpServer {
  const server = new McpServer({ name: 'thorvision-framelab', version: '0.2.0' });
  let workbench: Promise<ControlWorkbench> | undefined;
  const controls = () => workbench ??= (async () => {
    const { projectFingerprint, finalizeProject } = await import('../preparation.js');
    const panel = new ControlWorkbench({ device: context.device, baseUrl: context.device.url,
      workspace: context.workspace, camera: context.camera,
      selectCamera: (camera) => { context.camera = camera; },
      fingerprint: () => projectFingerprint(context.workspace),
      candidate: () => {
        try {
          const directory = projectPath(context, '');
          if (!existsSync(join(directory, 'manifest.json'))) return { dirty: false };
          const manifest = JSON.parse(readFileSync(join(directory, 'manifest.json'), 'utf8'));
          const dirty = manifest.content_digest !== contentDigest(directory);
          return { name: manifest.name, digest: dirty ? undefined : packageDigest(directory), dirty };
        } catch (error) { return { dirty: true, error: (error as Error).message }; }
      },
      isPreparing: () => preparations.has(context),
      prepare: (progress, signal) => prepareExtension(context, progress, signal),
      finalize: (started, progress, signal) => finalizeProject(context, started, progress, signal),
    });
    await panel.start();
    return panel;
  })().catch((error) => { workbench = undefined; throw error; });
  server.server.onclose = () => { if (workbench) void workbench.then((panel) => panel.close()).catch(() => {}); };
  const checkOperation = async (editing = false) => {
    if (workbench && (await workbench).changingDevice) throw new Error('Wait for the reviewed device operation to finish.');
    if (editing && preparations.has(context)) throw new Error('Wait for preparation to finish before editing or testing another revision.');
  };
  const guard = (run: () => Promise<Result> | Result) => async (): Promise<Result> => {
    try {
      return await run();
    } catch (error) {
      const message = error instanceof DeviceError && error.status === 0 ? `device unavailable: ${error.message}` : (error as Error).message;
      return text(message, true);
    }
  };

  server.registerTool('open_control_panel', {
    description: 'Open the local human control panel and live processed video. Returns a session-local URL. Tell the user to open it to review and run a tested candidate. Never call the human control HTTP actions yourself.',
    annotations: { readOnlyHint: true },
  }, guard(async () => text((await controls()).connection())));
  server.registerTool('get_control_status', {
    description: 'Read camera, candidate readiness, preparation progress and live processing measurements. Does not activate anything.',
    annotations: { readOnlyHint: true },
  }, guard(async () => text(await (await controls()).status())));

  server.registerTool('get_sdk_contract', {
    description: 'Callback contract, limits, package format and examples for ThorVision analytics extensions.',
    annotations: { readOnlyHint: true },
  }, guard(() => text(sdkContract(context))));

  server.registerTool('get_device_capabilities', {
    description: 'Device SDK/protocol versions, Python version, architecture, resource limits and the selected camera state.',
    annotations: { readOnlyHint: true },
  }, guard(async () => {
    const [capabilities, state] = await Promise.all([context.device.capabilities(), context.device.state()]);
    return text({ camera: context.camera, capabilities, camera_state: state.cameras?.[context.camera] ?? null });
  }));

  server.registerTool('get_sample_frames', {
    description: 'A current processed-preview frame of the selected camera when analytics is running there, plus the frame geometry. Raw camera frames are not exposed.',
    annotations: { readOnlyHint: true },
  }, guard(async () => {
    const frame = await context.device.previewFrame(context.camera);
    if (!frame) return text('No processed preview is running for this camera. Frames are fitted within 640x480; the deterministic fixture used by test_candidate is 1280x720 moving coloured boxes.');
    return {
      content: [
        { type: 'text', text: JSON.stringify(frame.header) },
        { type: 'image', data: frame.jpeg.toString('base64'), mimeType: 'image/jpeg' },
      ],
    };
  }));

  server.registerTool('read_project', {
    description: 'Read a file of the extension project, or list it when path is a directory or empty.',
    inputSchema: { path: z.string().default('').describe('Path relative to the extension project') },
    annotations: { readOnlyHint: true },
  }, async ({ path }) => guard(() => {
    const target = projectPath(context, path);
    if (!existsSync(target)) return text(path ? `${path} does not exist` : 'The project is empty.', !!path);
    if (lstatSync(target).isDirectory()) {
      const listing = readdirSync(target, { withFileTypes: true }).map((entry) => (entry.isDirectory() ? entry.name + '/' : entry.name));
      return text(listing.length ? listing.join('\n') : 'The project is empty.');
    }
    return text(readFileSync(target, 'utf8'));
  })());

  server.registerTool('write_project', {
    description: 'Create or replace a text file in the extension project. Any change makes earlier test results stale.',
    inputSchema: { path: z.string().min(1), content: z.string() },
  }, async ({ path, content }) => guard(async () => {
    await checkOperation(true);
    const target = projectPath(context, path);
    if (Buffer.byteLength(content) > MAX_FILE_BYTES) return text(`files are limited to ${MAX_FILE_BYTES >> 20} MiB`, true);
    const root = projectPath(context, '');
    if (!existsSync(target) && existsSync(root) && packageFiles(root).length >= MAX_FILES) return text(`the project is limited to ${MAX_FILES} files`, true);
    mkdirSync(dirname(target), { recursive: true });
    writeFileSync(target, content);
    record(context, { event: 'write', path });
    return text(`wrote ${path} (${Buffer.byteLength(content)} bytes)`);
  })());

  server.registerTool('validate_extension', {
    description: 'Check the manifest and package locally, record its content digest and return the package digest.',
  }, guard(async () => {
    await checkOperation(true);
    const directory = projectPath(context, '');
    const { problems } = validateManifest(directory, sdkVersion(context));
    if (problems.length) return text({ valid: false, problems }, true);
    const digest = seal(directory);
    record(context, { event: 'validate', digest });
    return text({ valid: true, digest });
  }));

  const prepare = async (extra: { signal: AbortSignal }) => {
    let last = '', sent = 0;
    const progress: Progress = (message) => {
      if (message === last || Date.now() - sent < 1000) return;
      last = message; sent = Date.now();
      record(context, { event: 'preparation_progress', message });
      void server.sendLoggingMessage({ level: 'info', logger: 'preparation', data: message }).catch(() => {});
    };
    return guard(async () => { await checkOperation(); return prepareExtension(context, progress, extra.signal); })();
  };
  server.registerTool('prepare_extension', {
    description: 'Prepare the completed extension: validate, automatically reuse compatible dependencies or download through this PC, and run the device test. Returns ready only after the exact candidate passes. Never activates it.',
  }, prepare);
  server.registerTool('prepare_dependencies', {
    description: 'Compatibility alias for prepare_extension: includes validation, automatic dependency reuse and device testing. No separate test request is necessary.',
  }, prepare);

  server.registerTool('test_candidate', {
    description: 'Upload the package and run it in the device sandbox on the deterministic fixture. Returns the test report; Apply requires a passing report for this exact digest.',
  }, guard(async () => { await checkOperation(true); return testCandidate(context, 'test'); }));

  server.registerTool('get_validation_report', {
    description: 'Test and preparation reports recorded for this extension project, newest first, or the one for a digest.',
    inputSchema: { digest: z.string().regex(/^[0-9a-f]{64}$/).optional() },
    annotations: { readOnlyHint: true },
  }, async ({ digest }) => guard(() => {
    const reports = join(context.workspace, 'history', 'reports');
    if (!existsSync(reports)) return text('No reports yet.');
    const names = readdirSync(reports).filter((name) => !digest || name.startsWith(digest));
    const values = names.map((name) => ({ name, ...JSON.parse(readFileSync(join(reports, name), 'utf8')), mtime: lstatSync(join(reports, name)).mtimeMs }));
    values.sort((a, b) => b.mtime - a.mtime);
    return text(values.slice(0, 5));
  })());

  return server;
}

export function contextFromEnvironment(): ToolContext {
  const workspace = process.env.THORVISION_WORKSPACE;
  const url = process.env.THORVISION_FRAMELAB_URL || process.env.THORVISION_EXTENSIONS_URL;
  if (!workspace || !url) throw new Error('THORVISION_WORKSPACE and THORVISION_FRAMELAB_URL (or legacy THORVISION_EXTENSIONS_URL) are required');
  mkdirSync(join(workspace, 'package'), { recursive: true });
  return {
    workspace,
    camera: process.env.THORVISION_CAMERA ?? '',
    device: new DeviceClient(url),
    snapshot: process.env.THORVISION_SDK_SNAPSHOT ?? resolve(import.meta.dirname, '..', '..', 'sdk-snapshot'),
  };
}

export async function startStdioServer(): Promise<void> {
  const server = createServer(contextFromEnvironment());
  await server.connect(new StdioServerTransport());
  // The SDK stdio transport does not close itself on EOF. A live-view HTTP
  // listener would otherwise outlive the native agent that owns this session.
  process.stdin.once('end', () => { void server.close(); });
  process.once('SIGTERM', () => { void server.close(); });
  process.once('SIGINT', () => { void server.close(); });
}

if (process.argv[1] && resolve(process.argv[1]) === resolve(import.meta.filename)) await startStdioServer();
