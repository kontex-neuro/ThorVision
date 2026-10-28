// ThorVision tools for every agent, served over MCP stdio by the provider runtime's MCP client.
//
// Environment: THORVISION_WORKSPACE (extension project), THORVISION_EXTENSIONS_URL (tunnel),
// THORVISION_CAMERA, THORVISION_SDK_SNAPSHOT. Agents edit only <workspace>/package; activation
// is not a tool: only the client's Apply can activate a tested digest.
import { appendFileSync, existsSync, lstatSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from 'node:fs';
import { dirname, isAbsolute, join, normalize, relative, resolve, sep } from 'node:path';
import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import { z } from 'zod';
import { DeviceClient, DeviceError } from '../device.js';
import { packTarGz, packageFiles, seal, validateManifest } from '../package.js';

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
    'source-image pixels (use frame.transform.box_to_source for processing-image boxes). Dependencies other than',
    'the Python standard library and thorvision_sdk cannot be staged yet; report that instead of inventing a workaround.',
    'Use validate_extension, then test_candidate, then get_validation_report. You cannot activate a package: tell the',
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

export async function testCandidate(context: ToolContext, kind: 'test' | 'prepare' = 'test'): Promise<Result> {
  const directory = projectPath(context, '');
  const { problems } = validateManifest(directory, sdkVersion(context));
  if (problems.length) return text({ passed: false, problems }, true);
  const digest = seal(directory);
  const uploaded = await context.device.upload(packTarGz(directory));
  if (uploaded.digest !== digest) return text({ passed: false, problems: [`device digest ${uploaded.digest} differs from local ${digest}`] }, true);
  const job = await context.device.job(kind, digest);
  const report = { kind, digest, state: job.state, ...(job.report ?? {}), error: job.error };
  saveReport(context, `${digest}.${kind}`, report);
  record(context, { event: kind, digest, state: job.state });
  return text(report, job.state !== 'succeeded');
}

export function createServer(context: ToolContext): McpServer {
  const server = new McpServer({ name: 'thorvision', version: '0.1.0' });
  const guard = (run: () => Promise<Result> | Result) => async (): Promise<Result> => {
    try {
      return await run();
    } catch (error) {
      const message = error instanceof DeviceError && error.status === 0 ? `device unavailable: ${error.message}` : (error as Error).message;
      return text(message, true);
    }
  };

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
  }, async ({ path, content }) => guard(() => {
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
  }, guard(() => {
    const directory = projectPath(context, '');
    const { problems } = validateManifest(directory, sdkVersion(context));
    if (problems.length) return text({ valid: false, problems }, true);
    const digest = seal(directory);
    record(context, { event: 'validate', digest });
    return text({ valid: true, digest });
  }));

  server.registerTool('prepare_dependencies', {
    description: 'Upload the package and ask the device to prepare its declared dependencies and models.',
  }, guard(() => testCandidate(context, 'prepare')));

  server.registerTool('test_candidate', {
    description: 'Upload the package and run it in the device sandbox on the deterministic fixture. Returns the test report; Apply requires a passing report for this exact digest.',
  }, guard(() => testCandidate(context, 'test')));

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
  const url = process.env.THORVISION_EXTENSIONS_URL;
  if (!workspace || !url) throw new Error('THORVISION_WORKSPACE and THORVISION_EXTENSIONS_URL are required');
  mkdirSync(join(workspace, 'package'), { recursive: true });
  return {
    workspace,
    camera: process.env.THORVISION_CAMERA ?? '',
    device: new DeviceClient(url),
    snapshot: process.env.THORVISION_SDK_SNAPSHOT ?? resolve(import.meta.dirname, '..', '..', 'sdk-snapshot'),
  };
}

if (process.argv[1] && resolve(process.argv[1]) === resolve(import.meta.filename)) {
  const server = createServer(contextFromEnvironment());
  await server.connect(new StdioServerTransport());
}
