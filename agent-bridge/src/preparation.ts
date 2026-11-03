import { existsSync, readFileSync, statSync } from 'node:fs';
import { join } from 'node:path';
import { contentDigest, seal } from './package.js';
import { DeviceClient } from './device.js';
import { prepareExtension, type ToolContext } from './tools/server.js';

export function projectFingerprint(workspace: string): string {
  const directory = join(workspace, 'package');
  if (!existsSync(directory)) return '';
  const manifest = join(directory, 'manifest.json');
  if (!existsSync(manifest)) return '';
  return contentDigest(directory) + (existsSync(manifest) ? readFileSync(manifest, 'utf8') : '');
}

/** Enforces preparation even when a provider forgets the authoring instructions. */
export async function finalizeProject(context: ToolContext, startedAt: number,
  progress: (message: string) => void, signal: AbortSignal): Promise<void> {
  const digest = seal(join(context.workspace, 'package'));
  const readyPath = join(context.workspace, 'history', 'reports', `${digest}.ready.json`);
  if (existsSync(readyPath)) {
    const report = JSON.parse(readFileSync(readyPath, 'utf8'));
    if (report.ready && report.digest === digest && report.tested_at >= startedAt) return;
  }
  for (const stage of ['prepare', 'test']) {
    const path = join(context.workspace, 'history', 'reports', `${digest}.${stage}.json`);
    if (!existsSync(path) || statSync(path).mtimeMs < startedAt) continue;
    const report = JSON.parse(readFileSync(path, 'utf8'));
    if (report.state === 'failed' || report.passed === false)
      throw new Error(`Preparation is blocked; this candidate is not ready to apply. ${report.error ?? report.fault?.message ?? `${stage} failed`}`);
  }
  const result = await prepareExtension(context, progress, signal);
  if (result.isError) {
    const detail = result.content.filter((item) => item.type === 'text').map((item) => item.text).join('\n');
    throw new Error(`Preparation failed; this candidate is not ready to apply. ${detail}`);
  }
}

/** Uploaded packages use the same mandatory prepare → test stages without a model call. */
export async function prepareUploaded(device: DeviceClient, digest: string,
  progress: (message: string) => void, signal: AbortSignal): Promise<unknown> {
  if (!/^[0-9a-f]{64}$/.test(digest)) throw new Error('A package digest is required');
  signal.throwIfAborted();
  progress('Preparing dependencies; compatible environments are reused automatically…');
  const prepared = await device.job('prepare', digest, 900000, { download: 'proxy' }, progress, signal);
  if (prepared.state !== 'succeeded' || !prepared.report?.passed)
    throw new Error(prepared.error ?? prepared.report?.error ?? 'Dependency preparation failed');
  signal.throwIfAborted();
  progress('Testing the prepared candidate on the device…');
  const tested = await device.job('test', digest, 300000, undefined, progress, signal);
  if (tested.state !== 'succeeded' || !tested.report?.passed)
    throw new Error(tested.error ?? tested.report?.error ?? tested.report?.fault?.message ?? 'Device test failed');
  if (tested.report.digest !== digest) throw new Error('Device test returned a different package digest');
  return { ...tested.report, ready: true, preparation: prepared.report };
}
