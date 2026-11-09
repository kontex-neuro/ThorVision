#!/usr/bin/env node
import { createHash, randomUUID } from 'node:crypto';
import { existsSync, mkdirSync, readFileSync, renameSync, writeFileSync } from 'node:fs';
import { homedir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { spawnSync } from 'node:child_process';
import { npmCli } from './npm.mjs';

const flags = {};
for (let n = 2; n < process.argv.length; n += 2) {
  if (!process.argv[n].startsWith('--') || !process.argv[n + 1]) throw new Error('Expected --artifact PATH_OR_HTTPS_URL --sha256 HASH [--prefix DIR].');
  flags[process.argv[n].slice(2)] = process.argv[n + 1];
}
function verifyCli(cli) {
  const verify = spawnSync(process.execPath, [cli, 'verify'], { encoding: 'utf8', windowsHide: true });
  if (verify.status !== 0) throw new Error(`Runtime file verification failed: ${verify.stderr.trim() || cli}`);
}
try {
  if (Number(process.versions.node.split('.')[0]) < 22) throw new Error('Node.js 22+ is required.');
  if (!flags.artifact || !/^[a-f0-9]{64}$/.test(flags.sha256 || '')) throw new Error('Provide an artifact and its expected SHA-256 from the trusted release manifest.');
  const prefix = resolve(flags.prefix || join(homedir(), '.thorvision/runtimes', flags.sha256.slice(0, 16)));
  const receiptPath = join(prefix, 'thorvision-install.json');
  if (existsSync(prefix)) {
    if (!existsSync(receiptPath) || JSON.parse(readFileSync(receiptPath, 'utf8')).artifactSha256 !== flags.sha256)
      throw new Error('The install directory already exists with different contents; choose a new versioned --prefix.');
    verifyCli(join(prefix, 'node_modules/@kontex/thorvision-framelab/bin/thorvision-framelab.mjs'));
    console.log(JSON.stringify({ installed: true, alreadyInstalled: true, prefix, cli: join(prefix, 'node_modules/@kontex/thorvision-framelab/bin/thorvision-framelab.mjs') }, null, 2));
  } else {
    let bytes;
    if (/^https:/i.test(flags.artifact)) {
      const url = new URL(flags.artifact);
      if (url.username || url.password) throw new Error('Download URLs must not contain credentials.');
      const response = await fetch(url, { signal: AbortSignal.timeout(180000) });
      if (!response.ok || !response.url.startsWith('https:')) throw new Error(`Release download failed: HTTP ${response.status}`);
      const chunks = [];
      let size = 0;
      for await (const chunk of response.body) {
        size += chunk.length;
        if (size > 64 * 1024 * 1024) throw new Error('Release archive exceeds 64 MiB.');
        chunks.push(chunk);
      }
      bytes = Buffer.concat(chunks);
    } else {
      if (/^[a-z]+:\/\//i.test(flags.artifact)) throw new Error('Use a local artifact file or HTTPS URL.');
      bytes = readFileSync(resolve(flags.artifact));
      if (bytes.length > 64 * 1024 * 1024) throw new Error('Release archive exceeds 64 MiB.');
    }
    if (createHash('sha256').update(bytes).digest('hex') !== flags.sha256) throw new Error('Release SHA-256 mismatch; nothing was installed.');
    const staging = join(dirname(prefix), `.thorvision-install-${randomUUID()}`);
    mkdirSync(staging, { recursive: true });
    const archive = join(staging, 'release.tgz');
    writeFileSync(archive, bytes);
    const result = spawnSync(process.execPath, [npmCli(), 'install', '--prefix', staging, '--omit=dev', '--ignore-scripts', '--no-audit', '--no-fund', archive],
      { stdio: 'inherit', windowsHide: true });
    if (result.status !== 0) throw new Error(`Installation failed; review the preserved staging directory: ${staging}`);
    const cli = join(staging, 'node_modules/@kontex/thorvision-framelab/bin/thorvision-framelab.mjs');
    try { verifyCli(cli); }
    catch (error) { throw new Error(`${error.message}. Staging preserved: ${staging}`); }
    writeFileSync(join(staging, 'thorvision-install.json'), JSON.stringify({ schema: 'thorvision.install/1', artifactSha256: flags.sha256, node: process.version }, null, 2) + '\n');
    renameSync(staging, prefix);
    console.log(JSON.stringify({ installed: true, prefix, cli: join(prefix, 'node_modules/@kontex/thorvision-framelab/bin/thorvision-framelab.mjs') }, null, 2));
  }
} catch (error) {
  console.error(error.message);
  process.exitCode = 1;
}
