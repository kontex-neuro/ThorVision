import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { mkdirSync, mkdtempSync, readFileSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { test } from 'node:test';
import { normalizePackageBins } from '../normalize-bins.mjs';
import { npmCli } from '../npm.mjs';

const deployment = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const hash = bytes => createHash('sha256').update(bytes).digest('hex');
const cliSource = readFileSync(join(deployment, 'runtime/bin/thorvision-framelab.mjs'), 'utf8')
  .replace(/\r?\n/g, '\r\n');

function node(args, cwd) {
  const result = spawnSync(process.execPath, args, { cwd, encoding: 'utf8', windowsHide: true });
  assert.equal(result.status, 0, result.stderr + result.stdout);
  return result.stdout;
}

test('real npm installation preserves pre-sealed normalized executables and binary payloads', () => {
  const root = mkdtempSync(join(tmpdir(), 'framelab npm bin '));
  const binary = Buffer.from([0, 13, 10, 255, 13, 10, 128]);
  for (const normalized of [false, true]) {
    const name = normalized ? 'normalized' : 'crlf-control';
    const source = join(root, name);
    mkdirSync(join(source, 'bin'), { recursive: true });
    writeFileSync(join(source, 'package.json'), JSON.stringify({ name: `framelab-bin-fixture-${name}`,
      version: '1.0.0', type: 'module', bin: { fixture: 'bin/thorvision-framelab.mjs' } }));
    writeFileSync(join(source, 'bin/thorvision-framelab.mjs'), cliSource);
    writeFileSync(join(source, 'binary.dat'), binary);
    if (normalized) normalizePackageBins(source);
    assert.deepEqual(readFileSync(join(source, 'binary.dat')), binary);
    const expected = hash(readFileSync(join(source, 'bin/thorvision-framelab.mjs')));
    writeFileSync(join(source, 'release.json'), JSON.stringify({ version: '1.0.0', filesSha256: 'fixture',
      files: { 'bin/thorvision-framelab.mjs': expected, 'binary.dat': hash(binary) } }));
    const packed = JSON.parse(node([npmCli(), 'pack', '--offline', '--ignore-scripts', '--json',
      '--cache', join(root, 'cache'), '--pack-destination', root], source))[0];
    const prefix = join(root, `installed-${name}`);
    node([npmCli(), 'install', '--prefix', prefix, '--offline', '--ignore-scripts', '--no-audit', '--no-fund',
      '--cache', join(root, 'cache'), join(root, packed.filename)], root);
    const installed = join(prefix, 'node_modules', `framelab-bin-fixture-${name}`);
    const cli = join(installed, 'bin/thorvision-framelab.mjs');
    const verified = spawnSync(process.execPath, [cli, 'verify'], { encoding: 'utf8', windowsHide: true });
    assert.deepEqual(readFileSync(join(installed, 'binary.dat')), binary);
    if (normalized) {
      assert.equal(hash(readFileSync(cli)), expected);
      assert.equal(verified.status, 0, verified.stderr);
      assert.equal(JSON.parse(verified.stdout).ok, true);
    } else {
      assert.notEqual(hash(readFileSync(cli)), expected);
      assert.notEqual(verified.status, 0);
      assert.match(verified.stderr, /Release file is missing or changed: bin\/thorvision-framelab.mjs/);
    }
  }
});
