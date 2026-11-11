import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, unlinkSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, resolve, sep } from 'node:path';
import { it, type TestContext } from 'node:test';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import { demoAuthoringInstructions, loadDemoCatalog, restoreDemo, verifyDemo } from '../demos.js';
import { DeviceClient } from '../device.js';
import { createServer } from '../tools/server.js';
import { contentDigest, packageDigest, seal } from '../package.js';

const hash = (data: Buffer) => createHash('sha256').update(data).digest('hex');
function save(root: string, name: string, value: string | Buffer) {
  const path = join(root, name);
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, value);
}
function snapshot(root: string): Record<string, string> {
  if (!existsSync(root)) return {};
  const entries: [string, string][] = [];
  function walk(directory: string, prefix = '') {
    for (const item of readdirSync(directory, { withFileTypes: true })) {
      const name = prefix + item.name, path = join(directory, item.name);
      if (item.isDirectory()) walk(path, name + '/');
      else entries.push([name, readFileSync(path).toString('base64')]);
    }
  }
  walk(root);
  return Object.fromEntries(entries);
}
function inventory(directory: string) {
  const hashes = Object.fromEntries(Object.entries(snapshot(directory))
    .filter(([name]) => name !== 'files.sha256.json')
    .map(([name, value]) => [name, hash(Buffer.from(value, 'base64'))]));
  save(directory, 'files.sha256.json', JSON.stringify(hashes));
}
function fixture(t: TestContext) {
  const root = mkdtempSync(join(tmpdir(), 'framelab demos ')), catalogRoot = join(root, 'demos');
  t.after(() => {
    assert.ok(resolve(root).startsWith(resolve(tmpdir()) + sep));
    rmSync(root, { recursive: true, force: true });
  });
  const options = ['person-detector', 'three-zone-entries'].map((id) => {
    const directory = join(catalogRoot, 'recipes', id), packageRoot = join(directory, 'package');
    save(packageRoot, 'processor.py', `# frozen implementation: ${id}\n`);
    save(packageRoot, 'config/defaults.json', '{}\n');
    save(packageRoot, 'manifest.json', JSON.stringify({ schema: 'thorvision.package/1', name: id,
      version: '1.0.0', sdk: '>=0.1,<0.2', entrypoint: 'processor:Processor' }));
    const digest = seal(packageRoot);
    save(directory, 'recipe.json', JSON.stringify({ schema: 'thorvision.lab-recipe/1', name: id,
      version: '1.0.0', config: {}, package: { path: 'package', digest, content_digest: contentDigest(packageRoot) } }));
    save(directory, 'assets/models/model.bin', Buffer.from([0, 1, 2, id.length]));
    save(directory, 'dependencies/requirements.lock', 'example==1.0.0\n');
    inventory(directory);
    return { id, label: id, prompt: `Restore exact ${id}`, recipe: 'recipes/' + id, digest };
  });
  save(catalogRoot, 'catalog.json', JSON.stringify({ schema: 'framelab.demo-catalog/1', options }));
  return { root, catalogRoot, options, workspace: join(root, 'workspace with spaces') };
}

it('a standard runtime has no demo catalog; demo catalogs require exactly the two distinct IDs', (t) => {
  const f = fixture(t);
  assert.equal(loadDemoCatalog(join(f.root, 'standard runtime')), undefined);
  assert.deepEqual(loadDemoCatalog(f.catalogRoot)?.options.map((item) => item.id), ['person-detector', 'three-zone-entries']);
  save(f.catalogRoot, 'catalog.json', JSON.stringify({ schema: 'framelab.demo-catalog/1', options: [f.options[0], f.options[0]] }));
  assert.throws(() => loadDemoCatalog(f.catalogRoot), /both distinct recipes/);
});

it('delivers exact demo routing through MCP initialization without putting it in the public prompts', async (t) => {
  const f = fixture(t), previous = process.env.THORVISION_DEMO_DIR;
  process.env.THORVISION_DEMO_DIR = f.catalogRoot;
  t.after(() => {
    if (previous === undefined) delete process.env.THORVISION_DEMO_DIR;
    else process.env.THORVISION_DEMO_DIR = previous;
  });
  const requests = ['Detect people in the camera view.', 'Count people entering three zones.'];
  save(f.catalogRoot, 'catalog.json', JSON.stringify({ schema: 'framelab.demo-catalog/1',
    options: f.options.map((item, index) => ({ ...item, prompt: requests[index] })) }));
  const server = createServer({ workspace: f.workspace, camera: '', snapshot: '',
    device: new DeviceClient('http://127.0.0.1:9/framelab') });
  const client = new Client({ name: 'plain-demo-test', version: '1.0.0' });
  const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
  await server.connect(serverTransport);
  try {
    await client.connect(clientTransport);
    const instructions = client.getInstructions();
    assert.ok(instructions);
    for (const item of f.options) {
      assert.ok(instructions.includes(item.id));
      assert.ok(instructions.includes(item.digest));
    }
    for (const request of requests) assert.ok(instructions.includes(request));
    assert.ok(instructions.includes('restore_demo_recipe'));
    assert.ok(instructions.includes('config {}'));
    assert.ok(instructions.includes('human-reviewed'));
    const publicCatalog = loadDemoCatalog(f.catalogRoot);
    assert.deepEqual(publicCatalog?.options.map((item) => item.prompt), requests);
    assert.equal(existsSync(f.workspace), false);
    process.env.THORVISION_DEMO_DIR = join(f.root, 'standard runtime');
    assert.equal(demoAuthoringInstructions(), undefined);
  } finally { await client.close(); await server.close(); }
});

it('restores exact sealed package bytes and supporting inventory without activation', (t) => {
  const f = fixture(t), selected = verifyDemo('person-detector', f.catalogRoot);
  const expected = snapshot(join(selected.directory, 'package'));
  const result = restoreDemo('person-detector', f.workspace, f.catalogRoot);
  assert.equal(result.activated, false);
  assert.equal(result.backup, null);
  assert.equal(result.digest, selected.recipe.package.digest);
  assert.equal(packageDigest(join(f.workspace, 'package')), selected.recipe.package.digest);
  assert.equal(contentDigest(join(f.workspace, 'package')), selected.recipe.package.content_digest);
  assert.deepEqual(snapshot(join(f.workspace, 'package')), expected);
  assert.deepEqual(JSON.parse(readFileSync(join(f.workspace, '.framelab-recipe/files.sha256.json'), 'utf8')), selected.hashes);
  assert.equal(readFileSync(join(f.workspace, '.framelab-recipe/assets/models/model.bin')).toString('hex'), '0001020f');
});

it('keeps a complete independently verifiable frozen recipe beside the separate writable package', (t) => {
  const f = fixture(t), selected = verifyDemo('person-detector', f.catalogRoot);
  const original = snapshot(selected.directory);
  const result = restoreDemo('person-detector', f.workspace, f.catalogRoot);
  const reference = join(f.workspace, '.framelab-recipe');
  assert.equal(result.recipe_directory, reference);
  assert.equal(result.package_directory, join(f.workspace, 'package'));
  assert.deepEqual(result.verification, { recipe_inventory_verified: true,
    recipe_inventory_files: Object.keys(selected.hashes).length,
    recipe_package_digest: selected.recipe.package.digest, recipe_content_digest: selected.recipe.package.content_digest,
    authoring_package_digest: selected.recipe.package.digest, authoring_content_digest: selected.recipe.package.content_digest });
  assert.deepEqual(snapshot(reference), original);
  for (const [name, expected] of Object.entries(selected.hashes)) {
    assert.equal(hash(readFileSync(join(reference, name))), expected, name);
  }
  assert.equal(packageDigest(join(reference, 'package')), selected.recipe.package.digest);
  assert.equal(contentDigest(join(reference, 'package')), selected.recipe.package.content_digest);
  save(f.workspace, 'package/processor.py', '# later authoring edit\n');
  assert.notEqual(contentDigest(join(f.workspace, 'package')), selected.recipe.package.content_digest);
  assert.deepEqual(snapshot(reference), original);
});

it('refuses changed, missing, unlisted and internally resealed recipe files before mutating the project', (t) => {
  for (const failure of ['changed', 'missing', 'unlisted', 'inventory-rehashed', 'package-resealed']) {
    const f = fixture(t), source = join(f.catalogRoot, 'recipes/person-detector');
    save(f.workspace, 'package/existing.py', '# preserve user edits\n');
    const before = snapshot(f.workspace);
    if (failure === 'missing') unlinkSync(join(source, 'package/processor.py'));
    else if (failure === 'unlisted') save(source, 'package/extra.py', '# unexpected\n');
    else {
      save(source, 'package/processor.py', '# changed implementation\n');
      if (failure === 'package-resealed') seal(join(source, 'package'));
      if (failure === 'inventory-rehashed' || failure === 'package-resealed') inventory(source);
    }
    assert.throws(() => restoreDemo('person-detector', f.workspace, f.catalogRoot), /inventory changed|file changed|identity differs/);
    assert.deepEqual(snapshot(f.workspace), before, failure);
    assert.equal(existsSync(join(f.workspace, 'history')), false);
  }
});

it('switching both recipes preserves prior work and every previous backup', (t) => {
  const f = fixture(t);
  save(f.workspace, 'package/user-analysis.py', '# valuable existing work\n');
  save(f.workspace, '.framelab-recipe/prior.json', '{"custom":true}\n');
  const oldPackage = snapshot(join(f.workspace, 'package')), oldSupport = snapshot(join(f.workspace, '.framelab-recipe'));
  const first = restoreDemo('person-detector', f.workspace, f.catalogRoot);
  assert.ok(first.backup);
  assert.deepEqual(snapshot(join(first.backup, 'package')), oldPackage);
  assert.deepEqual(snapshot(join(first.backup, 'recipe')), oldSupport);
  const firstBackup = snapshot(first.backup), firstPackage = snapshot(join(f.workspace, 'package'));
  const second = restoreDemo('three-zone-entries', f.workspace, f.catalogRoot);
  assert.ok(second.backup);
  assert.notEqual(first.backup, second.backup);
  assert.deepEqual(snapshot(join(second.backup, 'package')), firstPackage);
  assert.equal(packageDigest(join(f.workspace, 'package')), verifyDemo('three-zone-entries', f.catalogRoot).selected.digest);
  const secondBackup = snapshot(second.backup);
  restoreDemo('person-detector', f.workspace, f.catalogRoot);
  assert.equal(packageDigest(join(f.workspace, 'package')), verifyDemo('person-detector', f.catalogRoot).selected.digest);
  assert.deepEqual(snapshot(first.backup), firstBackup);
  assert.deepEqual(snapshot(second.backup), secondBackup);
});

it('unsafe recipe paths and unsupported IDs cannot escape the bundled catalog', (t) => {
  const f = fixture(t);
  for (const path of ['../outside', '/outside', 'C:/outside', 'recipes//person-detector', 'recipes/./person-detector']) {
    const options = f.options.map((item) => item.id === 'person-detector' ? { ...item, recipe: path } : item);
    save(f.catalogRoot, 'catalog.json', JSON.stringify({ schema: 'framelab.demo-catalog/1', options }));
    assert.throws(() => restoreDemo('person-detector', f.workspace, f.catalogRoot), /Invalid demo file path|escaped/);
    assert.equal(existsSync(f.workspace), false);
  }
  assert.throws(() => verifyDemo('another-demo', f.catalogRoot), /two bundled demo recipes/);
});
