import { createHash, randomUUID } from 'node:crypto';
import { cpSync, existsSync, lstatSync, mkdirSync, readdirSync, readFileSync, renameSync, rmdirSync } from 'node:fs';
import { isAbsolute, join, relative, resolve, sep } from 'node:path';
import { z } from 'zod';
import { contentDigest, packageDigest } from './package.js';

const sha = z.string().regex(/^[a-f0-9]{64}$/);
const demoId = z.enum(['person-detector', 'three-zone-entries']);
const catalogSchema = z.object({
  schema: z.literal('framelab.demo-catalog/1'),
  options: z.array(z.object({ id: demoId, label: z.string().min(1), prompt: z.string().min(1),
    recipe: z.string().min(1), digest: sha })).length(2),
}).refine((value) => new Set(value.options.map((item) => item.id)).size === 2, 'The demo edition requires both distinct recipes.');
const recipeSchema = z.object({ schema: z.literal('thorvision.lab-recipe/1'), name: z.string(), version: z.string(),
  config: z.object({}).strict(), package: z.object({ path: z.literal('package'), digest: sha, content_digest: sha }).passthrough(),
}).passthrough();
type Catalog = z.infer<typeof catalogSchema>;

function beneath(root: string, name: string): string {
  if (isAbsolute(name) || name.split(/[\\/]/).some((part) => !part || part === '.' || part === '..' || part.includes(':')))
    throw new Error('Invalid demo file path');
  const path = resolve(root, name);
  if (!path.startsWith(resolve(root) + sep)) throw new Error('Demo path escaped its root');
  let current = resolve(root);
  if (existsSync(current) && lstatSync(current).isSymbolicLink()) throw new Error('Demo links are not allowed');
  for (const part of relative(root, path).split(sep)) {
    current = join(current, part);
    if (existsSync(current) && lstatSync(current).isSymbolicLink()) throw new Error('Demo links are not allowed');
  }
  return path;
}

function files(root: string, prefix = ''): string[] {
  return readdirSync(join(root, prefix), { withFileTypes: true }).flatMap((entry) => {
    const name = prefix ? `${prefix}/${entry.name}` : entry.name;
    const path = beneath(root, name), info = lstatSync(path);
    if (!info.isFile() && !info.isDirectory()) throw new Error('Unsupported demo file');
    return info.isDirectory() ? files(root, name) : [name];
  });
}

export function demoDirectory(): string {
  return resolve(process.env.THORVISION_DEMO_DIR || resolve(import.meta.dirname, '..', 'demos'));
}

export function loadDemoCatalog(root = demoDirectory()): Catalog | undefined {
  if (!existsSync(join(root, 'catalog.json'))) return undefined;
  return catalogSchema.parse(JSON.parse(readFileSync(beneath(root, 'catalog.json'), 'utf8')));
}

/** Agent context, kept separate from the plain requests shown in the demo picker. */
export function demoAuthoringInstructions(catalog = loadDemoCatalog()): string | undefined {
  if (!catalog) return undefined;
  return [
    'This is the FrameLab two-demo edition. The user-facing requests below select frozen implementations.',
    'For a matching request, call get_demo_recipes, then restore_demo_recipe with the mapped ID before doing any authoring.',
    'Preserve the restored callback bytes and config {}. Do not regenerate implementation code from the user wording.',
    ...catalog.options.map((item) => `${JSON.stringify(item.prompt)} selects ${JSON.stringify(item.id)}; expected package digest ${item.digest}.`),
    'If the request selects both demos without an order, ask which one to use. For explicit custom changes, distinguish a modified analysis from exact demo reproduction.',
    'Inspect the SDK and target capabilities, then prepare_extension and verify the exact package, model and dependency identities.',
    'Report environment differences without rewriting frozen identities or claiming that the full device environment is identical.',
    'Keep recipe IDs, hashes and tool calls in agent context and technical reports; the user does not need to put them in the prompt.',
    'Preserve any running analysis. Run, Stop, Restore and cleanup remain human-reviewed panel actions; never invoke them yourself.',
  ].join('\n');
}

function verifyRecipeDirectory(directory: string, expectedDigest: string) {
  const hashes = z.record(z.string(), sha).parse(JSON.parse(readFileSync(beneath(directory, 'files.sha256.json'), 'utf8')));
  const inventory = files(directory).filter((name) => name !== 'files.sha256.json').sort();
  if (JSON.stringify(inventory) !== JSON.stringify(Object.keys(hashes).sort())) throw new Error('Demo recipe inventory changed.');
  for (const [name, expected] of Object.entries(hashes)) {
    if (createHash('sha256').update(readFileSync(beneath(directory, name))).digest('hex') !== expected)
      throw new Error(`Demo recipe file changed: ${name}`);
  }
  const recipe = recipeSchema.parse(JSON.parse(readFileSync(beneath(directory, 'recipe.json'), 'utf8')));
  const packageRoot = beneath(directory, 'package');
  const manifest = z.object({ content_digest: sha }).passthrough().parse(JSON.parse(readFileSync(beneath(packageRoot, 'manifest.json'), 'utf8')));
  if (packageDigest(packageRoot) !== expectedDigest || recipe.package.digest !== expectedDigest
    || contentDigest(packageRoot) !== recipe.package.content_digest || manifest.content_digest !== recipe.package.content_digest)
    throw new Error('Demo package identity differs from its recipe.');
  return { directory, recipe, hashes };
}

export function verifyDemo(id: string, root = demoDirectory()) {
  const selected = loadDemoCatalog(root)?.options.find((item) => item.id === id);
  if (!selected) throw new Error('Choose one of the two bundled demo recipes.');
  return { selected, ...verifyRecipeDirectory(beneath(root, selected.recipe), selected.digest) };
}

/** Restore frozen bytes locally. Existing work is retained in history; no device action occurs. */
export function restoreDemo(id: string, workspace: string, root = demoDirectory()) {
  const verified = verifyDemo(id, root);
  const project = beneath(workspace, 'package'), support = beneath(workspace, '.framelab-recipe');
  if (existsSync(project)) files(project);
  if (existsSync(support)) files(support);
  const stage = beneath(workspace, `.demo-stage-${randomUUID()}`);
  const backup = beneath(workspace, `history/demo-imports/${Date.now()}-${randomUUID()}`);
  mkdirSync(stage, { recursive: true });
  cpSync(join(verified.directory, 'package'), join(stage, 'package'), { recursive: true });
  mkdirSync(join(stage, 'support'));
  for (const name of Object.keys(verified.hashes).concat('files.sha256.json')) {
    const target = beneath(join(stage, 'support'), name);
    mkdirSync(resolve(target, '..'), { recursive: true });
    cpSync(beneath(verified.directory, name), target);
  }
  const reference = verifyRecipeDirectory(join(stage, 'support'), verified.selected.digest);
  const authoringDigest = packageDigest(join(stage, 'package'));
  const authoringContentDigest = contentDigest(join(stage, 'package'));
  if (authoringDigest !== reference.recipe.package.digest || authoringContentDigest !== reference.recipe.package.content_digest)
    throw new Error('Restored authoring package differs from the complete frozen recipe.');
  const moved: { from: string; to: string }[] = [];
  let installedProject = false, installedSupport = false;
  try {
    for (const [from, label] of [[project, 'package'], [support, 'recipe']]) {
      if (!existsSync(from)) continue;
      mkdirSync(backup, { recursive: true });
      const to = join(backup, label);
      renameSync(from, to); moved.push({ from, to });
    }
    renameSync(join(stage, 'package'), project); installedProject = true;
    renameSync(join(stage, 'support'), support); installedSupport = true;
  } catch (error) {
    if (installedSupport) renameSync(support, join(stage, 'support'));
    if (installedProject) renameSync(project, join(stage, 'package'));
    for (const item of moved.reverse()) renameSync(item.to, item.from);
    throw error;
  }
  rmdirSync(stage);
  return { restored: true, id, name: verified.recipe.name, version: verified.recipe.version,
    digest: verified.selected.digest, config: verified.recipe.config, workspace,
    recipe_directory: support, package_directory: project,
    verification: { recipe_inventory_verified: true, recipe_inventory_files: Object.keys(reference.hashes).length,
      recipe_package_digest: reference.recipe.package.digest, recipe_content_digest: reference.recipe.package.content_digest,
      authoring_package_digest: authoringDigest, authoring_content_digest: authoringContentDigest },
    backup: moved.length ? backup : null, activated: false,
    next: 'recipe_directory is the complete frozen reference, including package/; its unchanged tools/recipe.py verifier can run there. package_directory is the separate writable callback copy. Preserve its bytes and empty configuration for exact reproduction. Call prepare_extension, compare its actual package/model/dependency identities with recipe_directory, then open the panel for the user to review Run on camera.' };
}
