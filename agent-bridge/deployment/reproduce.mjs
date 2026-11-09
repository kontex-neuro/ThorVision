#!/usr/bin/env node
import { createHash } from 'node:crypto';
import { readFileSync } from 'node:fs';
import { homedir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

try {
  const flags = {};
  const args = process.argv.slice(2);
  for (let n = 0; n < args.length; n++) {
    const key = args[n].replace(/^--/, '');
    if (!['agent', 'workspace', 'device-url', 'prefix', 'home', 'replace-skill', 'check-device'].includes(key)) throw new Error(`Unknown option: ${args[n]}`);
    if (['replace-skill', 'check-device'].includes(key)) flags[key] = true;
    else {
      if (!args[n + 1] || args[n + 1].startsWith('--')) throw new Error(`Missing value for ${args[n]}`);
      flags[key] = args[++n];
    }
  }
  if (!flags.agent || !flags.workspace || !flags['device-url']) throw new Error('Use --agent NAME --workspace DIR --device-url URL [--prefix DIR].');
  const kit = dirname(fileURLToPath(import.meta.url));
  const recipe = JSON.parse(readFileSync(join(kit, 'release-manifest.json'), 'utf8'));
  for (const [name, digest] of Object.entries(recipe.installerFiles || {})) {
    if (createHash('sha256').update(readFileSync(join(kit, name))).digest('hex') !== digest) throw new Error(`Installer file changed: ${name}`);
  }
  const prefix = resolve(flags.prefix || join(homedir(), '.thorvision/runtimes', recipe.sha256.slice(0, 16)));
  const run = (script, args) => {
    const result = spawnSync(process.execPath, [script, ...args], { stdio: 'inherit', windowsHide: true });
    if (result.status !== 0) throw new Error(`Setup stopped with exit ${result.status}; see the error above.`);
  };
  run(join(kit, 'install.mjs'), ['--artifact', join(kit, recipe.artifact), '--sha256', recipe.sha256, '--prefix', prefix]);
  const cli = join(prefix, 'node_modules/@kontex/thorvision-framelab/bin/thorvision-framelab.mjs');
  run(cli, ['setup', '--agent', flags.agent, '--workspace', flags.workspace, '--device-url', flags['device-url'],
    ...(flags.home ? ['--home', flags.home] : []), ...(flags['replace-skill'] ? ['--replace-skill'] : [])]);
  run(cli, ['doctor', '--workspace', flags.workspace, ...(flags['check-device'] ? [] : ['--offline']), '--json']);
  console.log('Ready for agent startup. Sign in/select your model in the agent, then ask it to open the ThorVision FrameLab control panel.');
} catch (error) {
  console.error(error.message);
  process.exitCode = 1;
}
