import { readFileSync, writeFileSync } from 'node:fs';
import { join, resolve, sep } from 'node:path';

export function normalizePackageBins(directory) {
  const root = resolve(directory);
  const { bin = {} } = JSON.parse(readFileSync(join(root, 'package.json'), 'utf8'));
  for (const name of typeof bin === 'string' ? [bin] : Object.values(bin)) {
    const path = resolve(root, name);
    if (!path.startsWith(root + sep)) throw new Error('Package executable escaped its staging directory.');
    const source = readFileSync(path, 'utf8');
    if (!source.startsWith('#!')) throw new Error(`Package executable has no shebang: ${name}`);
    // npm fixes CRLF shebangs during installation. Seal the portable bytes first.
    writeFileSync(path, source.replace(/\r\n/g, '\n'), 'utf8');
  }
}
