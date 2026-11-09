import { existsSync, realpathSync } from 'node:fs';
import { delimiter, dirname, join, resolve } from 'node:path';

// Invoke npm through Node directly. This avoids shell quoting and npm.cmd issues on Windows.
export function npmCli() {
  const candidates = [process.env.npm_execpath,
    join(dirname(process.execPath), 'node_modules/npm/bin/npm-cli.js'),
    resolve(dirname(process.execPath), '../lib/node_modules/npm/bin/npm-cli.js')];
  for (const path of (process.env.PATH || '').split(delimiter)) {
    candidates.push(join(path, 'node_modules/npm/bin/npm-cli.js'));
    const executable = join(path, 'npm');
    if (existsSync(executable)) candidates.push(realpathSync(executable));
  }
  for (const path of candidates) if (path && path.endsWith('.js') && existsSync(path)) return path;
  throw new Error('npm-cli.js was not found. Install Node.js with npm or set npm_execpath.');
}
