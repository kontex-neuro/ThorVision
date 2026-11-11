// Pieces every adapter shares: the ThorVision tool server and the authoring instructions.
import { existsSync, readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { dirname, join, resolve } from 'node:path';
import type { SessionOptions } from '../types.js';

export const BRIDGE_ROOT = resolve(import.meta.dirname, '..', '..');
export const TOOLS_SERVER = join(BRIDGE_ROOT, 'dist', 'tools', 'server.js');
export const SDK_SNAPSHOT = join(BRIDGE_ROOT, 'sdk-snapshot');
export const TOOL_NAMES = [
  'get_sdk_contract', 'get_device_capabilities', 'get_sample_frames', 'read_project', 'write_project',
  'validate_extension', 'prepare_extension', 'prepare_dependencies', 'test_candidate', 'get_validation_report',
  'get_digital_io', 'set_digital_outputs',
];

export function toolsServer(options: SessionOptions) {
  return {
    command: process.execPath,
    args: [TOOLS_SERVER],
    env: {
      THORVISION_WORKSPACE: options.workspace,
      THORVISION_EXTENSIONS_URL: options.extensionsUrl,
      THORVISION_FRAMELAB_URL: options.extensionsUrl,
      THORVISION_CAMERA: options.camera,
      THORVISION_SDK_SNAPSHOT: SDK_SNAPSHOT,
    },
  };
}

export const INSTRUCTIONS = [
  'You build ThorVision FrameLab workflows: a Python callback that runs on the device for one camera',
  'and returns structured detections and drawings. Use only the framelab tools. Start with',
  'get_sdk_contract and get_device_capabilities, keep every file inside the extension project with',
  'write_project, then call prepare_extension after the final edit. Preparation automatically validates, reuses',
  'compatible dependencies or downloads missing ones, and tests the exact revision. Finish by reporting the tested digest',
  'and what the user should check in the preview. You cannot activate a package; the user applies it.',
  'When external dependencies or model weights are needed, declare them in the manifest and call',
  'prepare_extension. These are mandatory product workflow steps: never ask the user to specify dependency reuse,',
  'validation, testing or a device connection in their prompt. Do not repeat a successful preparation without edits.',
  'The developer pre-deploys the device service. All user operations use its API; never request SSH login,',
  'deploy the service or fall back to SSH when the API is unavailable. Report the API error for the developer.',
  'Report preparation errors; never substitute simulated detections. Only a passing preparation is ready to apply.',
  'Digital I/O tools operate on the selected device separately from callback authoring. Use get_digital_io to inspect',
  'available input/output bits (1 HIGH, 0 LOW). set_digital_outputs uses a mask and values; it reads fresh status and sends',
  'one revision-checked write. Report conflicts or uncertain completion without automatic retries. Output readback is an',
  'FPGA latch, not connector voltage. Callbacks gain no hardware or network access and cannot invoke these tools.',
  'If tools are deferred, use the provided functions.exec wrapper to discover ThorVision entries in ALL_TOOLS',
  'and invoke only the corresponding tools.mcp__framelab__ methods (legacy thorvision registrations are supported). Discover before reporting tools unavailable.',
].join(' ');

/** JS entry point of an npm package's binary, launched with this Node so no global install is used. */
export function packageBin(name: string, bin: string): string {
  const require = createRequire(import.meta.url);
  const manifestPath = require.resolve(`${name}/package.json`);
  const manifest = require(manifestPath);
  const relative = typeof manifest.bin === 'string' ? manifest.bin : manifest.bin[bin];
  return join(dirname(manifestPath), relative);
}

/** Installed version; reads package.json beside the entry point because some packages do not export it. */
export function packageVersion(name: string): string {
  const require = createRequire(import.meta.url);
  try {
    return require(`${name}/package.json`).version;
  } catch {
    // Fall through: the package does not export package.json.
  }
  let directory = dirname(require.resolve(name));
  for (;;) {
    const candidate = join(directory, 'package.json');
    if (existsSync(candidate)) {
      const manifest = JSON.parse(readFileSync(candidate, 'utf8'));
      if (manifest.name === name) return manifest.version;
    }
    const parent = dirname(directory);
    if (parent === directory) throw new Error(`${name}: package.json not found`);
    directory = parent;
  }
}
