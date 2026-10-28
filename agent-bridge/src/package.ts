// Extension package handling, matching thorvision_sdk.package (digests must agree byte for byte).
import { createHash } from 'node:crypto';
import { lstatSync, readdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join, relative, sep } from 'node:path';
import { gzipSync } from 'node:zlib';

export const PACKAGE_SCHEMA = 'thorvision.package/1';
export const MANIFEST = 'manifest.json';
const NAME = /^[a-z0-9][a-z0-9_-]{0,63}$/;
const HEX64 = /^[0-9a-f]{64}$/;
const MAX_PACKAGE_BYTES = 64 << 20;

/** Files in the same order as Python's os.walk with sorted names: a directory's files, then its subdirectories. */
export function packageFiles(directory: string): string[] {
  const found: string[] = [];
  const walk = (folder: string) => {
    const entries = readdirSync(folder, { withFileTypes: true });
    const names = entries.map((entry) => entry.name).sort(byCodePoint);
    for (const name of names) {
      const path = join(folder, name);
      const stat = lstatSync(path);
      const rel = relative(directory, path).split(sep).join('/');
      if (stat.isSymbolicLink()) throw new Error(`${rel}: symbolic links are not allowed`);
      if (stat.isFile() && rel !== MANIFEST && !name.endsWith('.pyc')) found.push(rel);
    }
    for (const name of names) {
      const path = join(folder, name);
      if (lstatSync(path).isDirectory() && name !== '__pycache__' && name !== '.git') walk(path);
    }
  };
  walk(directory);
  return found;
}

function byCodePoint(a: string, b: string): number {
  return a < b ? -1 : a > b ? 1 : 0;
}

export function contentDigest(directory: string): string {
  const digest = createHash('sha256');
  for (const rel of packageFiles(directory)) {
    const data = readFileSync(join(directory, rel));
    digest.update(Buffer.concat([Buffer.from(`${rel}\0${data.length}\0`, 'utf8'), data]));
  }
  return digest.digest('hex');
}

export function packageDigest(directory: string): string {
  return createHash('sha256').update(readFileSync(join(directory, MANIFEST))).digest('hex');
}

/** Python json.dump(indent=2, sort_keys=True) with ensure_ascii, plus a trailing newline. */
export function pythonJson(value: unknown, indent = ''): string {
  if (value === null || typeof value !== 'object') {
    return JSON.stringify(value).replace(/[\u007f-￿]/g, (c) => '\\u' + c.charCodeAt(0).toString(16).padStart(4, '0'));
  }
  const inner = indent + '  ';
  if (Array.isArray(value)) {
    if (!value.length) return '[]';
    return '[\n' + value.map((item) => inner + pythonJson(item, inner)).join(',\n') + '\n' + indent + ']';
  }
  const keys = Object.keys(value as object).sort(byCodePoint);
  if (!keys.length) return '{}';
  return '{\n' + keys.map((key) => inner + pythonJson(key) + ': ' + pythonJson((value as any)[key], inner)).join(',\n') + '\n' + indent + '}';
}

/** Records content_digest in the manifest and returns the resulting package digest. */
export function seal(directory: string): string {
  const path = join(directory, MANIFEST);
  const manifest = JSON.parse(readFileSync(path, 'utf8'));
  manifest.content_digest = contentDigest(directory);
  writeFileSync(path, pythonJson(manifest) + '\n');
  return packageDigest(directory);
}

function version(text: string): number[] {
  return text.split('.').map((part) => Number.parseInt(part, 10));
}

export function satisfies(current: string, requirement: string): boolean {
  const have = version(current);
  for (const clause of requirement.split(',')) {
    const match = /^\s*(>=|<=|==|<|>)\s*(\d+(?:\.\d+)*)\s*$/.exec(clause);
    if (!match) throw new Error(`unsupported requirement clause ${JSON.stringify(clause)}`);
    const want = version(match[2]);
    const width = Math.max(have.length, want.length);
    let order = 0;
    for (let i = 0; i < width && !order; i++) order = Math.sign((have[i] ?? 0) - (want[i] ?? 0));
    const ok = { '>=': order >= 0, '<=': order <= 0, '==': order === 0, '<': order < 0, '>': order > 0 }[match[1]];
    if (!ok) return false;
  }
  return true;
}

/** The same checks as thorvision_sdk.package.load_manifest, before content_digest. */
export function validateManifest(directory: string, sdkVersion: string): { manifest: any; problems: string[] } {
  const problems: string[] = [];
  let manifest: any;
  try {
    manifest = JSON.parse(readFileSync(join(directory, MANIFEST), 'utf8'));
  } catch (error) {
    return { manifest: null, problems: [`${MANIFEST}: ${(error as Error).message}`] };
  }
  const need = (key: string) => {
    if (typeof manifest[key] !== 'string') problems.push(`${MANIFEST}: ${key} is required`);
    return manifest[key];
  };
  if (manifest.schema !== PACKAGE_SCHEMA) problems.push(`${MANIFEST}: schema must be ${PACKAGE_SCHEMA}`);
  const name = need('name');
  if (typeof name === 'string' && !NAME.test(name)) problems.push(`${MANIFEST}: name must be lowercase letters, digits, "_" or "-"`);
  need('version');
  const entrypoint = need('entrypoint');
  const requirement = need('sdk');
  if (typeof requirement === 'string') {
    try {
      if (!satisfies(sdkVersion, requirement)) problems.push(`${MANIFEST}: requires SDK ${requirement}, device provides ${sdkVersion}`);
    } catch (error) {
      problems.push(`${MANIFEST}: ${(error as Error).message}`);
    }
  }
  const files = new Set(safeFiles(directory, problems));
  if (typeof entrypoint === 'string') {
    const [module, attribute] = entrypoint.split(':');
    if (!module || !attribute || !files.has(module.replaceAll('.', '/') + '.py'))
      problems.push(`${MANIFEST}: entrypoint ${JSON.stringify(entrypoint)} must be "module:Class" inside the package`);
  }
  for (const key of ['config_schema', 'dependencies']) {
    if (manifest[key] != null && !files.has(manifest[key])) problems.push(`${MANIFEST}: ${key} file ${JSON.stringify(manifest[key])} is missing`);
  }
  for (const [index, model] of (manifest.models ?? []).entries()) {
    for (const key of ['name', 'path', 'sha256', 'source', 'license'])
      if (typeof model?.[key] !== 'string') problems.push(`${MANIFEST}: models[${index}].${key} is required`);
    if (typeof model?.sha256 === 'string' && !HEX64.test(model.sha256))
      problems.push(`${MANIFEST}: models[${index}].sha256 must be 64 lowercase hex digits`);
  }
  for (const path of manifest.fixtures ?? []) if (!files.has(path)) problems.push(`${MANIFEST}: fixture ${JSON.stringify(path)} is missing`);
  return { manifest, problems };
}

function safeFiles(directory: string, problems: string[]): string[] {
  try {
    const files = packageFiles(directory);
    const total = files.reduce((sum, rel) => sum + lstatSync(join(directory, rel)).size, 0);
    if (total > MAX_PACKAGE_BYTES) problems.push(`package exceeds ${MAX_PACKAGE_BYTES >> 20} MiB`);
    return files;
  } catch (error) {
    problems.push((error as Error).message);
    return [];
  }
}

/** POSIX ustar archive (regular files only) of the package, gzip-compressed for upload. */
export function packTarGz(directory: string): Buffer {
  const blocks: Buffer[] = [];
  for (const rel of [MANIFEST, ...packageFiles(directory)]) {
    const data = readFileSync(join(directory, rel));
    let name = rel;
    let prefix = '';
    if (Buffer.byteLength(name) > 99) {
      const cut = rel.lastIndexOf('/', 154);
      if (cut <= 0 || Buffer.byteLength(rel.slice(cut + 1)) > 99) throw new Error(`${rel}: path too long for the package archive`);
      prefix = rel.slice(0, cut);
      name = rel.slice(cut + 1);
    }
    const header = Buffer.alloc(512);
    const field = (offset: number, length: number, text: string) => header.write(text, offset, length, 'utf8');
    field(0, 100, name);
    field(100, 8, '0000644\0');
    field(108, 8, '0000000\0');
    field(116, 8, '0000000\0');
    field(124, 12, data.length.toString(8).padStart(11, '0') + '\0');
    field(136, 12, '00000000000\0');
    header.fill(' ', 148, 156);
    field(156, 1, '0');
    field(257, 6, 'ustar\0');
    field(263, 2, '00');
    field(345, 155, prefix);
    let checksum = 0;
    for (const byte of header) checksum += byte;
    field(148, 8, checksum.toString(8).padStart(6, '0') + '\0 ');
    blocks.push(header, data, Buffer.alloc((512 - (data.length % 512)) % 512));
  }
  blocks.push(Buffer.alloc(1024));
  return gzipSync(Buffer.concat(blocks));
}
