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
  'validate_extension', 'prepare_dependencies', 'test_candidate', 'get_validation_report',
];

export function toolsServer(options: SessionOptions) {
  return {
    command: process.execPath,
    args: [TOOLS_SERVER],
    env: {
      THORVISION_WORKSPACE: options.workspace,
      THORVISION_EXTENSIONS_URL: options.extensionsUrl,
      THORVISION_CAMERA: options.camera,
      THORVISION_SDK_SNAPSHOT: SDK_SNAPSHOT,
    },
  };
}

export const INSTRUCTIONS = [
  'You write ThorVision analytics extensions: a Python callback that runs on the device for one camera',
  'and returns structured detections and drawings. Use only the thorvision tools. Start with',
  'get_sdk_contract and get_device_capabilities, keep every file inside the extension project with',
  'write_project, run validate_extension and test_candidate, and finish by reporting the tested digest',
  'and what the user should check in the preview. You cannot activate a package; the user applies it.',
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
