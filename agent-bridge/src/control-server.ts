// Human-facing loopback UI. The model's MCP surface never executes activation or cleanup.
import { randomBytes } from 'node:crypto';
import { readFileSync } from 'node:fs';
import { createServer, type IncomingMessage, type ServerResponse } from 'node:http';
import { resolve } from 'node:path';
import { ControlSession, type ControlAction } from './control.js';
import { DeviceClient } from './device.js';
import { loadDemoCatalog } from './demos.js';

type PrepareResult = { content: { type: string; text?: string }[]; isError?: boolean };
export interface WorkbenchOptions {
  device: DeviceClient;
  baseUrl: string;
  workspace: string;
  camera: string;
  selectCamera: (camera: string) => void;
  fingerprint: () => string;
  candidate: () => { name?: string; digest?: string; dirty: boolean; error?: string };
  isPreparing?: () => boolean;
  prepare: (progress: (message: string) => void, signal: AbortSignal) => Promise<PrepareResult>;
  finalize: (startedAt: number, progress: (message: string) => void, signal: AbortSignal) => Promise<void>;
}

export class ControlWorkbench {
  readonly control: ControlSession;
  private readonly token = randomBytes(32).toString('hex');
  private readonly server = createServer((request, response) => { void this.handle(request, response); });
  private port = 0;
  private busy?: AbortController;
  private mutating = false;
  private reserving = false;
  private authoring?: { fingerprint: string; startedAt: number };
  private operation = { state: 'idle', message: 'Describe your processing idea in your agent.', updatedAt: Date.now() };
  private stream?: WebSocket;
  private streamCamera = '';
  private frame?: { jpeg: Buffer; header: any; receivedAt: number };
  private retry?: ReturnType<typeof setTimeout>;
  private closed = false;
  private staticRoot = resolve(import.meta.dirname, '..', 'control-ui');

  constructor(private readonly options: WorkbenchOptions) {
    this.control = new ControlSession(options.device, { camera: options.camera, workspace: options.workspace });
  }

  async start(): Promise<{ url: string; api: string; token: string }> {
    await new Promise<void>((resolvePromise, reject) => {
      this.server.once('error', reject);
      this.server.listen(0, '127.0.0.1', () => { this.server.off('error', reject); resolvePromise(); });
    });
    this.port = (this.server.address() as { port: number }).port;
    return this.connection();
  }

  connection() {
    return { url: `http://localhost:${this.port}/#${this.token}`, api: `http://127.0.0.1:${this.port}`, token: this.token };
  }

  get changingDevice(): boolean { return this.mutating || this.reserving; }

  async close() {
    this.closed = true;
    this.busy?.abort();
    if (this.retry) clearTimeout(this.retry);
    this.stream?.close();
    this.server.closeAllConnections();
    await new Promise<void>((done) => this.server.close(() => done()));
  }

  private update(state: string, message: string) { this.operation = { state, message, updatedAt: Date.now() }; }

  private startPreparation(finalizeAt?: number) {
    if (this.busy) return;
    if (this.changingDevice || this.options.isPreparing?.()) throw new Error('Another device operation is in progress.');
    if (this.authoring && finalizeAt === undefined) throw new Error('Wait for Claude to finish editing before preparing.');
    const abort = new AbortController();
    this.busy = abort;
    this.update('preparing', 'Checking this revision, dependencies and device tests…');
    const progress = (message: string) => this.update('preparing', message);
    void (async () => {
      try {
        if (finalizeAt !== undefined) await this.options.finalize(finalizeAt, progress, abort.signal);
        else {
          const result = await this.options.prepare(progress, abort.signal);
          if (result.isError) throw new Error(result.content.map((item) => item.text ?? '').join('\n'));
        }
        abort.signal.throwIfAborted();
        this.update('ready', 'Device tests passed. Review the candidate, then run it on your camera.');
      } catch (error) {
        this.update(abort.signal.aborted ? 'cancelled' : 'failed', (error as Error).message);
      } finally { if (this.busy === abort) this.busy = undefined; }
    })();
  }

  private ensurePreview(camera: string) {
    if (this.streamCamera === camera && this.stream) return;
    if (this.retry) clearTimeout(this.retry);
    this.streamCamera = camera;
    this.stream?.close();
    this.frame = undefined;
    if (!camera || this.closed) { this.stream = undefined; return; }
    const socket = new WebSocket(`${this.options.baseUrl.replace(/^http/, 'ws')}/v1/cameras/${encodeURIComponent(camera)}/preview`);
    this.stream = socket;
    socket.binaryType = 'arraybuffer';
    socket.onmessage = (event) => {
      if (this.stream !== socket) return;
      try {
        const bytes = Buffer.from(event.data as ArrayBuffer);
        if (bytes.length < 8 || bytes.length > 8 * 1024 * 1024 || bytes.subarray(0, 4).toString() !== 'TVP1') return;
        const size = bytes.readUInt32LE(4);
        if (size > 65536 || 8 + size >= bytes.length) return;
        this.frame = { header: JSON.parse(bytes.subarray(8, 8 + size).toString()), jpeg: bytes.subarray(8 + size), receivedAt: Date.now() };
      } catch { /* An invalid frame is not displayed. */ }
    };
    socket.onerror = () => socket.close();
    socket.onclose = () => {
      if (this.stream !== socket || this.closed) return;
      this.stream = undefined;
      this.retry = setTimeout(() => this.ensurePreview(camera), 2000);
    };
  }

  async status() {
    const snapshot = await this.control.snapshot();
    const detected = snapshot.cameras.filter((camera) => camera.discovered);
    if (!detected.some((camera) => camera.id === snapshot.camera) && detected.length === 1) {
      await this.control.selectCamera(detected[0].id);
      snapshot.camera = this.control.camera;
    }
    const camera = snapshot.camera ?? '';
    if (camera !== this.options.camera) { this.options.camera = camera; this.options.selectCamera(camera); }
    this.ensurePreview(camera);
    let fingerprint = '', projectError = '';
    try { fingerprint = this.options.fingerprint(); } catch (error) { projectError = (error as Error).message; }
    return { ...snapshot, demo: loadDemoCatalog(), capabilities: { sdk: snapshot.capabilities?.sdk },
      candidate: this.options.candidate(),
      operation: this.operation, busy: !!this.busy || this.changingDevice || !!this.options.isPreparing?.(),
      canCancel: !!this.busy, authoring: !!this.authoring,
      hasProject: !!fingerprint, projectError, preview: this.frame ? { ...this.frame.header, receivedAt: this.frame.receivedAt } : null };
  }

  private json(response: ServerResponse, status: number, value: unknown) {
    response.writeHead(status, { 'content-type': 'application/json; charset=utf-8' });
    response.end(JSON.stringify(value));
  }

  private async body(request: IncomingMessage) {
    if (!request.headers['content-type']?.startsWith('application/json')) throw new Error('JSON is required');
    const chunks: Buffer[] = [];
    let size = 0;
    for await (const chunk of request) {
      size += chunk.length;
      if (size > 65536) throw new Error('Request exceeds 64 KiB');
      chunks.push(chunk);
    }
    return JSON.parse(Buffer.concat(chunks).toString() || '{}');
  }

  private async handle(request: IncomingMessage, response: ServerResponse) {
    response.setHeader('Cache-Control', 'no-store');
    response.setHeader('Referrer-Policy', 'no-referrer');
    response.setHeader('X-Content-Type-Options', 'nosniff');
    response.setHeader('Content-Security-Policy', "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' blob:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'");
    try {
      const hosts = [`127.0.0.1:${this.port}`, `localhost:${this.port}`];
      if (!hosts.includes(request.headers.host ?? '')) return this.json(response, 403, { error: 'Invalid local host' });
      if (request.headers.origin && !hosts.map((host) => 'http://' + host).includes(request.headers.origin))
        return this.json(response, 403, { error: 'Foreign origin refused' });
      const path = new URL(request.url ?? '/', 'http://localhost').pathname;
      if (request.method === 'GET' && ['/', '/app.js', '/style.css'].includes(path)) {
        const name = path === '/' ? 'index.html' : path.slice(1);
        response.setHeader('Content-Type', name.endsWith('.js') ? 'text/javascript' : name.endsWith('.css') ? 'text/css' : 'text/html');
        response.end(readFileSync(resolve(this.staticRoot, name)));
        return;
      }
      if (request.headers.authorization !== `Bearer ${this.token}`) return this.json(response, 401, { error: 'Open the live view from the ThorVision panel again.' });
      if (request.method === 'GET' && path === '/api/status') return this.json(response, 200, await this.status());
      if (request.method === 'GET' && path === '/api/frame') {
        this.ensurePreview(this.control.camera ?? '');
        if (!this.frame || Date.now() - this.frame.receivedAt > 4000) { response.writeHead(204); response.end(); return; }
        response.setHeader('Content-Type', 'image/jpeg');
        response.setHeader('X-Frame-Time', String(this.frame.receivedAt));
        response.setHeader('X-Frame-Sequence', String(this.frame.header.frame?.sequence ?? ''));
        response.setHeader('X-Frame-Kind', this.frame.header.kind === 'raw' ? 'raw' : 'processed');
        response.end(this.frame.jpeg);
        return;
      }
      // The native panel's fetch reads text only, so it takes the same frame as base64 JSON.
      if (request.method === 'GET' && path === '/api/frame.json') {
        this.ensurePreview(this.control.camera ?? '');
        if (!this.frame || Date.now() - this.frame.receivedAt > 4000) return this.json(response, 200, { frame: null });
        return this.json(response, 200, { frame: { jpeg: this.frame.jpeg.toString('base64'), receivedAt: this.frame.receivedAt,
          sequence: String(this.frame.header.frame?.sequence ?? ''), width: this.frame.header.width, height: this.frame.header.height,
          kind: this.frame.header.kind === 'raw' ? 'raw' : 'processed' } });
      }
      if (request.method !== 'POST') return this.json(response, 404, { error: 'Unknown operation' });
      const body = await this.body(request);
      if (path === '/api/authoring/begin') {
        if (this.busy || this.changingDevice || this.options.isPreparing?.())
          throw new Error('A device operation is in progress. Wait for it to finish.');
        // Only this session's mod begins turns, so an open window means the last turn never finished.
        // Keep its baseline so the next finish still prepares edits made by the lost turn.
        if (this.authoring) return this.json(response, 200, { ok: true, resumed: true });
        this.reserving = true;
        try {
          await this.status();
          if (!this.control.camera) throw new Error('Select a camera in /framelab first.');
          this.authoring = { fingerprint: this.options.fingerprint(), startedAt: Date.now() };
          this.update('editing', 'Claude is working on your processing idea.');
        } finally { this.reserving = false; }
        return this.json(response, 200, { ok: true });
      }
      if (path === '/api/authoring/finish') {
        const started = this.authoring;
        this.authoring = undefined;
        if (body.aborted) this.update('cancelled', 'Editing was interrupted. Prepare when you are ready.');
        else if (started && this.options.fingerprint() !== started.fingerprint) this.startPreparation(started.startedAt);
        else if (started) this.update('idle', 'No new processing revision was created.');
        return this.json(response, 200, { operation: this.operation });
      }
      if (path === '/api/cancel') {
        if (!this.busy) throw new Error('No panel preparation is cancellable. Interrupt an agent tool in Claude.');
        this.busy.abort();
        this.update('cancelling', 'Cancellation requested. A device job already started may still finish; activation stays unchanged.');
        return this.json(response, 200, { ok: true });
      }
      if (path === '/api/prepare') { this.startPreparation(); return this.json(response, 202, { operation: this.operation }); }
      if (this.busy || this.changingDevice || this.options.isPreparing?.() || this.authoring) throw new Error('Wait for editing and device operations to finish.');
      if (path === '/api/camera') {
        this.reserving = true;
        try {
          await this.control.selectCamera(String(body.camera ?? ''));
          this.options.camera = this.control.camera ?? '';
          this.options.selectCamera(this.options.camera);
          this.ensurePreview(this.options.camera);
        } finally { this.reserving = false; }
        return this.json(response, 200, { camera: this.options.camera });
      }
      if (path === '/api/review') return this.json(response, 200, await this.control.review({ action: body.action as ControlAction, digest: body.digest, camera: this.control.camera }));
      if (path === '/api/disconnected/review') {
        if (typeof body.camera !== 'string') throw new Error('Choose a disconnected analysis to stop');
        return this.json(response, 200, await this.control.review({ action: 'stop', camera: body.camera }));
      }
      if (path === '/api/cleanup/review') {
        if (!Array.isArray(body.digests) || !body.digests.every((value: unknown) => typeof value === 'string')) throw new Error('Select packages to review');
        return this.json(response, 200, await this.control.reviewCleanup(body.digests, body.includeUnusedCache === true));
      }
      if (path === '/api/execute') {
        this.mutating = true;
        this.update('running-operation', 'Applying your reviewed change on the device…');
        // Worker startup can take a minute. Return promptly to native mod hosts,
        // whose HTTP calls have a shorter deadline; status carries the outcome.
        void this.control.execute(String(body.id ?? '')).then(() => {
          this.update('idle', 'Device operation completed.');
        }, (error: Error) => this.update('failed', error.message)).finally(() => { this.mutating = false; });
        return this.json(response, 202, { operation: this.operation });
      }
      return this.json(response, 404, { error: 'Unknown operation' });
    } catch (error) { return this.json(response, 409, { error: (error as Error).message }); }
  }
}
