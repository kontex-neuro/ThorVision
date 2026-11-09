import { createHash } from 'node:crypto';
import { cpSync, existsSync, mkdirSync, readFileSync, readdirSync, rmSync, statSync, writeFileSync } from 'node:fs';
import { dirname, join, relative, resolve, sep } from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { npmCli } from './npm.mjs';

const deploy = dirname(fileURLToPath(import.meta.url));
const bridge = dirname(deploy);
const output = join(bridge, '.release-build');
const stage = join(output, 'package');
const readJson = path => JSON.parse(readFileSync(path, 'utf8'));
const pretty = value => JSON.stringify(value, null, 2) + '\n';
const hash = value => createHash('sha256').update(value).digest('hex');
const args = process.argv.slice(2);
const skillIndex = args.indexOf('--skill-dir');
const skill = resolve(skillIndex >= 0 ? args[skillIndex + 1] : process.env.THORVISION_FRAMELAB_SKILLS_SOURCE || process.env.THORVISION_SKILLS_SOURCE || join(deploy, 'skills/thorvision-framelab'));
const legacySkill = join(dirname(skill), 'thorvision-analytics');
if (!existsSync(join(skill, 'SKILL.md')) || !existsSync(join(legacySkill, 'SKILL.md')))
  throw new Error('The skill snapshot is missing. Provide --skill-dir DIR with its sibling thorvision-analytics compatibility skill.');

function files(root, prefix = '') {
  return readdirSync(join(root, prefix), { withFileTypes: true }).sort((a, b) => a.name.localeCompare(b.name, 'en')).flatMap(entry => {
    if (['node_modules', '__pycache__', '.git'].includes(entry.name) || /\.(?:pyc|map)$/.test(entry.name)) return [];
    const name = prefix ? `${prefix}/${entry.name}` : entry.name;
    if (entry.isSymbolicLink()) throw new Error(`Source links are not packaged: ${name}`);
    return entry.isDirectory() ? files(root, name) : [name];
  });
}
function hashes(root) { return Object.fromEntries(files(root).map(path => [path, hash(readFileSync(join(root, path)))])); }
function git(root) {
  const run = args => spawnSync('git', ['-C', root, ...args], { encoding: 'utf8', windowsHide: true });
  const head = run(['rev-parse', 'HEAD']);
  const status = run(['status', '--porcelain', '--untracked-files=normal']);
  return { head: head.status === 0 ? head.stdout.trim() : null, dirty: status.status !== 0 || !!status.stdout.trim() };
}
function runNode(args, cwd) {
  const result = spawnSync(process.execPath, args, { cwd, stdio: 'inherit', windowsHide: true });
  if (result.status !== 0) throw new Error(`Build command failed with exit ${result.status}: ${args[0]}`);
}

// These are fixed generated paths within this checkout, never caller-provided deletion targets.
for (const path of [join(output, 'compiled'), stage]) {
  if (!resolve(path).startsWith(resolve(output) + sep)) throw new Error('Generated output escaped build directory.');
  if (existsSync(path)) rmSync(path, { recursive: true });
}
mkdirSync(stage, { recursive: true });
runNode([join(bridge, 'node_modules/typescript/bin/tsc'), '-p', join(deploy, 'tsconfig.json')], bridge);
cpSync(join(output, 'compiled'), join(stage, 'dist'), { recursive: true });
for (const name of ['control-ui', 'sdk-snapshot']) {
  cpSync(join(bridge, name), join(stage, name), { recursive: true,
    filter: path => !path.split(/[\\/]/).includes('__pycache__') && !path.endsWith('.pyc') });
}
cpSync(join(deploy, 'runtime/bin'), join(stage, 'bin'), { recursive: true });
for (const name of ['package.json', 'npm-shrinkwrap.json', 'README.md']) cpSync(join(deploy, 'runtime', name), join(stage, name));
// npm consumes/removes shrinkwrap metadata when installing a tarball. Keep an immutable audit copy.
cpSync(join(deploy, 'runtime/npm-shrinkwrap.json'), join(stage, 'dependency-lock.json'));
cpSync(join(bridge, '../LICENSE'), join(stage, 'LICENSE'));
cpSync(join(deploy, 'stack.recipe.json'), join(stage, 'stack.recipe.json'));
for (const [name, source] of [['thorvision-framelab', skill], ['thorvision-analytics', legacySkill]]) {
  for (const path of files(source)) {
    const target = join(stage, 'skills', name, path);
    mkdirSync(dirname(target), { recursive: true });
    cpSync(join(source, path), target);
  }
}
const sourceFiles = Object.fromEntries([
  ...files(join(bridge, 'src')).filter(path => !path.startsWith('protocol/') && !path.startsWith('test/')).map(path => [`src/${path}`, hash(readFileSync(join(bridge, 'src', path)))]),
  ...files(join(bridge, 'control-ui')).map(path => [`control-ui/${path}`, hash(readFileSync(join(bridge, 'control-ui', path)))]),
  ...files(deploy).filter(path => !path.startsWith('test/')).map(path => [`deployment/${path}`, hash(readFileSync(join(deploy, path)))]),
]);
const content = hashes(stage);
delete content['npm-shrinkwrap.json'];
const release = {
  schema: 'thorvision.mcp-release/1', version: readJson(join(stage, 'package.json')).version,
  status: 'local-candidate-not-published', source: { repository: 'kontex-neuro/xvc', ...git(bridge), treeSha256: hash(pretty(sourceFiles)) },
  skillSource: { repository: skill === resolve(join(deploy, 'skills/thorvision-framelab')) ? 'kontex-neuro/xvc' : 'external-override', ...git(skill), directory: relative(resolve(join(bridge, '..')), skill).split(sep).join('/'),
    treeSha256: hash(pretty(hashes(skill))), compatibilityAliasTreeSha256: hash(pretty(hashes(legacySkill))) },
  sdk: readJson(join(stage, 'sdk-snapshot/SNAPSHOT.json')),
  dependenciesSha256: hash(readFileSync(join(stage, 'npm-shrinkwrap.json'))),
  filesSha256: hash(pretty(content)), files: content,
};
writeFileSync(join(stage, 'release.json'), pretty(release));
console.log(`Standalone MCP staged: ${stage}`);
console.log(`Runtime source dirty: ${release.source.dirty}; skill source dirty: ${release.skillSource.dirty}. Candidate hashes are recorded; commit/pin before publication.`);
if (args.includes('--pack')) {
  const artifacts = join(bridge, 'release-artifacts');
  mkdirSync(artifacts, { recursive: true });
  const packed = spawnSync(process.execPath, [npmCli(), 'pack', '--ignore-scripts', '--json', '--pack-destination', artifacts], {
    cwd: stage, encoding: 'utf8', windowsHide: true,
  });
  if (packed.status !== 0) throw new Error(packed.stderr || 'npm pack failed');
  const result = JSON.parse(packed.stdout)[0];
  const path = join(artifacts, result.filename);
  const sha256 = hash(readFileSync(path));
  writeFileSync(`${path}.sha256`, `${sha256}  ${result.filename}\n`);
  const record = { artifact: result.filename, sha256, bytes: statSync(path).size, release };
  const kit = join(artifacts, `deploy-kit-${release.version}-${sha256.slice(0, 12)}`);
  mkdirSync(kit, { recursive: true });
  const installerFiles = {};
  for (const name of ['install.mjs', 'npm.mjs', 'reproduce.mjs']) {
    cpSync(join(deploy, name), join(kit, name));
    installerFiles[name] = hash(readFileSync(join(kit, name)));
  }
  record.installerFiles = installerFiles;
  cpSync(path, join(kit, result.filename));
  cpSync(`${path}.sha256`, join(kit, `${result.filename}.sha256`));
  for (const name of ['README.md', 'MATERIALS.md', 'DEVICE-DEPLOYMENT.md', 'stack.recipe.json']) cpSync(join(deploy, name), join(kit, name));
  writeFileSync(join(kit, 'release-manifest.json'), pretty(record));
  writeFileSync(join(artifacts, 'release-manifest.json'), pretty(record));
  console.log(pretty({ artifact: path, sha256, bytes: record.bytes, deployKit: kit }));
}
