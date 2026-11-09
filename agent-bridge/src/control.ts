import { randomUUID } from 'node:crypto';
import { DeviceClient, DeviceError } from './device.js';

export type ControlAction = 'apply' | 'stop' | 'rollback';
export interface ControlCamera {
  id: string;
  name: string;
  discovered: boolean;
  state: any;
}
export interface ControlSnapshot {
  camera?: string;
  workspace?: string;
  cameras: ControlCamera[];
  packages: any[];
  state: any;
  capabilities: any;
  cameraDiscoveryError?: string;
}
export interface ActionReview {
  id: string;
  action: ControlAction;
  camera: string;
  digest?: string;
  config: Record<string, unknown>;
  revision: number;
  activeDigest: string | null;
  expiresAt: number;
}
export interface CleanupReview {
  id: string;
  action: 'cleanup';
  expiresAt: number;
  plan: any;
}
export type ControlReview = ActionReview | CleanupReview;
interface StoredReview {
  view: ControlReview;
  activation?: string;
  execution?: Promise<any>;
}

const DIGEST = /^[0-9a-f]{64}$/;
function fail(message: string): never { throw new DeviceError(409, message); }
function canonical(value: any): string {
  if (Array.isArray(value)) return '[' + value.map(canonical).join(',') + ']';
  if (value && typeof value === 'object') return '{' + Object.keys(value).sort()
    .map((key) => JSON.stringify(key) + ':' + canonical(value[key])).join(',') + '}';
  return JSON.stringify(value) ?? 'null';
}
function activation(entry: any): string {
  return canonical({ revision: entry.revision, active: entry.active ?? null, previous: entry.previous ?? null });
}

/** Fixed-device controls shared by human-facing hosts. A review token binds an
 * action; it is NOT evidence of consent. The host must present that exact review
 * and obtain a human action before calling execute. Preparation is caller-owned.
 */
export class ControlSession {
  private selectedCamera?: string;
  private readonly reviews = new Map<string, StoredReview>();

  constructor(readonly device: DeviceClient, readonly options: {
    camera?: string; workspace?: string; reviewTtlMs?: number; now?: () => number;
  } = {}) {
    this.selectedCamera = options.camera;
  }

  get camera(): string | undefined { return this.selectedCamera; }
  private now(): number { return this.options.now?.() ?? Date.now(); }

  async snapshot(): Promise<ControlSnapshot> {
    let cameraDiscoveryError: string | undefined;
    const [state, catalog, capabilities, discovered] = await Promise.all([
      this.device.state(), this.device.packages(), this.device.capabilities(),
      this.device.cameras().catch((error: Error) => { cameraDiscoveryError = error.message; return []; }),
    ]);
    if (!state?.cameras || typeof state.cameras !== 'object' || Array.isArray(state.cameras)
      || !Array.isArray(catalog?.packages)) fail('device returned an invalid control catalog');
    const cameras = new Map<string, ControlCamera>();
    for (const item of discovered) {
      // Only stable_id is compatible with analytics FrameRefs. Numeric camera IDs
      // and device_id aliases must never silently replace it.
      if (typeof item?.stable_id === 'string' && item.stable_id) cameras.set(item.stable_id, {
        id: item.stable_id, name: typeof item.name === 'string' ? item.name : item.stable_id,
        discovered: true, state: state.cameras[item.stable_id] ?? { revision: 0, active: null, previous: null },
      });
    }
    for (const [id, entry] of Object.entries<any>(state.cameras)) if (!cameras.has(id)) {
      // Old failed requests can leave empty activation records. With a current
      // hardware catalog, these are not selectable cameras. Keep actual prior
      // activations visible so a disconnected camera can still be stopped.
      if (discovered.length && !entry?.active && !entry?.previous && !entry?.run) continue;
      // With a working catalog, a missing ID is a disconnected or re-enumerated camera. Name it
      // so the human can recognise it and Stop it; the full path stays in `id`.
      const name = discovered.length ? `Disconnected camera (${id.split('/').pop() || id})` : id;
      cameras.set(id, { id, name, discovered: false, state: entry });
    }
    return { camera: this.selectedCamera, workspace: this.options.workspace, cameras: [...cameras.values()],
      packages: catalog.packages, state, capabilities, ...(cameraDiscoveryError ? { cameraDiscoveryError } : {}) };
  }

  async cameras(): Promise<ControlCamera[]> { return (await this.snapshot()).cameras; }

  async selectCamera(camera: string): Promise<void> {
    if (!(await this.cameras()).some((item) => item.id === camera)) fail('camera is not present in the device catalog');
    this.selectedCamera = camera;
  }

  private save(view: ControlReview, signature?: string): ControlReview {
    for (const [id, entry] of this.reviews) if (entry.view.expiresAt <= this.now()) this.reviews.delete(id);
    if (this.reviews.size >= 64) fail('too many outstanding control reviews');
    this.reviews.set(view.id, { view: structuredClone(view), activation: signature });
    return structuredClone(view);
  }

  async review(request: { action: ControlAction; camera?: string; digest?: string }): Promise<ActionReview> {
    if (!['apply', 'stop', 'rollback'].includes(request.action)) fail('unknown control action');
    const snapshot = await this.snapshot();
    const camera = request.camera ?? this.selectedCamera;
    const found = snapshot.cameras.find((item) => item.id === camera);
    if (!found) fail('select a camera from the device catalog first');
    const entry = found.state;
    if (!Number.isSafeInteger(entry?.revision) || entry.revision < 0) fail('camera revision is unavailable');
    let digest: string | undefined;
    let config: Record<string, unknown> = {};
    if (request.action === 'stop') {
      if (!entry.active) fail('camera has no active extension');
      digest = entry.active.digest;
    } else {
      if (request.action === 'rollback') {
        if (!entry.previous?.digest) fail('camera has no previous extension to restore');
        digest = entry.previous.digest;
        config = entry.previous.config ?? {};
      } else digest = request.digest;
      if (!digest || !DIGEST.test(digest)) fail('an exact package digest is required');
      if (!snapshot.packages.some((item) => item.digest === digest && item.ready === true)) {
        fail('package is not ready for the current device runtime; prepare and test it first');
      }
      if (Object.keys(config).length) fail('the device currently supports tests with empty configuration only');
    }
    return this.save({ id: randomUUID(), action: request.action, camera: found.id, digest,
      config, revision: entry.revision, activeDigest: entry.active?.digest ?? null,
      expiresAt: this.now() + (this.options.reviewTtlMs ?? 300000) }, activation(entry)) as ActionReview;
  }

  async reviewCleanup(packages: string[], includeUnusedCache = false): Promise<CleanupReview> {
    if (!Array.isArray(packages) || packages.length > 256 || packages.some((digest) => !DIGEST.test(digest))
      || typeof includeUnusedCache !== 'boolean') fail('cleanup requires exact package digests and a cache choice');
    const selected = [...new Set(packages)].sort();
    const plan = await this.device.cleanupPlan(selected, includeUnusedCache);
    if (typeof plan?.id !== 'string' || !Array.isArray(plan.blocked) || !Array.isArray(plan.packages)
      || canonical([...plan.packages].sort()) !== canonical(selected)
      || plan.include_unused_cache !== includeUnusedCache || !Number.isFinite(plan.expires_at)) {
      fail('device returned an invalid cleanup plan');
    }
    return this.save({ id: randomUUID(), action: 'cleanup',
      expiresAt: Math.min(plan.expires_at * 1000, this.now() + (this.options.reviewTtlMs ?? 300000)), plan }) as CleanupReview;
  }

  /** Repeated/concurrent execution returns the same attempt, including failures.
   * A failed or uncertain request requires a new review, not an automatic retry.
   */
  execute(id: string): Promise<any> {
    const stored = this.reviews.get(id);
    if (!stored) return Promise.reject(new DeviceError(409, 'control review is missing; review the action first'));
    if (stored.execution) return stored.execution;
    if (stored.view.expiresAt <= this.now()) return Promise.reject(new DeviceError(409, 'control review expired'));
    stored.execution = this.executeReviewed(stored);
    return stored.execution;
  }

  private async executeReviewed(stored: StoredReview): Promise<any> {
    const review = stored.view;
    if (review.action === 'cleanup') {
      if (review.plan.blocked.length) fail('cleanup is blocked by protected packages or running jobs');
      // The server atomically revalidates its full reference/filesystem snapshot.
      return this.device.cleanupExecute(review.plan.id);
    }
    const snapshot = await this.snapshot();
    if (review.expiresAt <= this.now()) fail('control review expired while checking device state');
    const current = snapshot.cameras.find((item) => item.id === review.camera);
    if (!current || activation(current.state) !== stored.activation) fail('camera activation changed; review the action again');
    if (review.action === 'stop') return this.device.stop(review.camera, review.revision);
    if (!snapshot.packages.some((item) => item.digest === review.digest && item.ready === true)) {
      fail('package readiness changed; prepare and review again');
    }
    return this.device.apply(review.camera, review.digest!, review.config, review.revision, review.id);
  }
}
