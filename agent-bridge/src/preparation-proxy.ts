// Temporary byte relay for the device's HTTPS CONNECT proxy. TLS remains end-to-end
// between the device and the repository; this process sees neither pip config nor TLS keys.
import { createConnection, type Socket } from 'node:net';
import { validateDeviceUrl } from './device-url.js';

export const PREPARATION_HOSTS = new Set([
  'pypi.org', 'files.pythonhosted.org', 'github.com', 'release-assets.githubusercontent.com',
]);
const CHUNK_BYTES = 48 * 1024;
const MAX_CONNECTIONS = 8;
const MAX_BUFFER_BYTES = 2 * 1024 * 1024;

export function validateProxyTarget(id: unknown, host: unknown, port: unknown): { id: string; host: string; port: number } {
  if (typeof id !== 'string' || !/^[a-zA-Z0-9_-]{1,64}$/.test(id)) throw new Error('invalid proxy connection id');
  if (typeof host !== 'string' || !PREPARATION_HOSTS.has(host.toLowerCase()) || port !== 443)
    throw new Error('preparation proxy permits only HTTPS package/model repositories');
  return { id, host: host.toLowerCase(), port };
}

export function decodeProxyChunk(value: unknown): Buffer {
  if (typeof value !== 'string' || value.length > CHUNK_BYTES * 4 / 3 || !/^[A-Za-z0-9+/]*={0,2}$/.test(value))
    throw new Error('invalid or oversized proxy data');
  const data = Buffer.from(value, 'base64');
  if (data.length > CHUNK_BYTES || data.toString('base64') !== value) throw new Error('invalid proxy data encoding');
  return data;
}

type Connector = (options: { host: string; port: number }) => Socket;

export function validateRelayUrl(value: string): void {
  const url = new URL(value);
  if (!['ws:', 'wss:'].includes(url.protocol) || url.username || url.password || url.hash ||
      !/^\/((?:framelab|analytics)\/)?v1\/jobs\/[a-f0-9]{32}\/proxy$/.test(url.pathname) ||
      !/^\?token=[a-zA-Z0-9_-]+$/.test(url.search))
    throw new Error('preparation proxy requires a token-bound device API relay');
  const prefix = ['/framelab', '/analytics'].find(value => url.pathname.startsWith(value + '/')) ?? '';
  validateDeviceUrl(url.origin.replace(/^ws/, 'http') + prefix);
}

export class PreparationProxy {
  private readonly sockets = new Map<string, Socket>();
  private readonly paused = new Set<Socket>();
  private readonly websocket: WebSocket;
  private intentionalClose = false;
  private finished = false;
  private failure?: Error;
  private heartbeat = Date.now();
  private readonly resumeTimer: ReturnType<typeof setInterval>;
  private readonly lifetimeTimer: ReturnType<typeof setTimeout>;
  private readonly ready: Promise<void>;
  private rejectReady: (error: Error) => void = () => {};
  private readonly abort = () => this.fail(new Error('preparation proxy cancelled'));

  constructor(url: string, progress: (message: string) => void = () => {},
    private readonly signal?: AbortSignal, private readonly connector: Connector = createConnection) {
    validateRelayUrl(url);
    this.websocket = new WebSocket(url);
    this.ready = new Promise<void>((resolve, reject) => {
      this.rejectReady = reject;
      const timer = setTimeout(() => this.fail(new Error('preparation proxy connection timed out')), 15000);
      this.websocket.onopen = () => { clearTimeout(timer); progress('Device connected through the PC internet proxy'); resolve(); };
      this.websocket.addEventListener('close', () => clearTimeout(timer));
    });
    this.websocket.onmessage = (event) => {
      try { this.receive(event.data); }
      catch (error) { this.fail(error as Error); }
    };
    this.websocket.onerror = () => this.fail(new Error('preparation proxy connection failed'));
    this.websocket.onclose = () => {
      if (!this.intentionalClose && !this.finished) this.fail(new Error('preparation proxy disconnected before completion'));
      else this.close();
    };
    this.resumeTimer = setInterval(() => {
      if (Date.now() - this.heartbeat > 20000) {
        try { this.send({ type: 'ping' }); this.heartbeat = Date.now(); }
        catch (error) { this.fail(error as Error); }
      }
      if (this.websocket.bufferedAmount > MAX_BUFFER_BYTES / 4) return;
      for (const socket of this.paused) socket.resume();
      this.paused.clear();
    }, 20);
    this.resumeTimer.unref();
    this.lifetimeTimer = setTimeout(() => this.fail(new Error('preparation proxy exceeded 15 minutes')), 900000);
    signal?.addEventListener('abort', this.abort, { once: true });
    if (signal?.aborted) this.abort();
  }

  async connected(): Promise<void> { await this.ready; }
  check(): void { if (this.failure) throw this.failure; }

  private send(message: unknown): void {
    if (this.websocket.readyState !== WebSocket.OPEN) return;
    if (this.websocket.bufferedAmount > MAX_BUFFER_BYTES) throw new Error('preparation proxy backpressure limit exceeded');
    this.websocket.send(JSON.stringify(message));
  }

  private receive(raw: unknown): void {
    if (typeof raw !== 'string' || raw.length > 70000) throw new Error('invalid proxy message');
    const message = JSON.parse(raw);
    if (message.type === 'finished') { this.finished = true; return; }
    if (message.type === 'open') {
      const { id, host, port } = validateProxyTarget(message.id, message.host, message.port);
      if (this.sockets.has(id) || this.sockets.size >= MAX_CONNECTIONS) throw new Error('proxy connection limit exceeded');
      const socket = this.connector({ host, port });
      this.sockets.set(id, socket);
      socket.setTimeout(60000);
      socket.on('connect', () => {
        try { this.send({ type: 'opened', id }); }
        catch (error) { this.fail(error as Error); }
      });
      socket.on('data', (data: Buffer) => {
        try {
          for (let offset = 0; offset < data.length; offset += CHUNK_BYTES)
            this.send({ type: 'data', id, data: data.subarray(offset, offset + CHUNK_BYTES).toString('base64') });
          if (this.websocket.bufferedAmount > MAX_BUFFER_BYTES / 4) { socket.pause(); this.paused.add(socket); }
        } catch (error) { this.fail(error as Error); }
      });
      socket.on('timeout', () => socket.destroy(new Error('repository connection timed out')));
      socket.on('error', (error) => {
        try { this.send({ type: 'error', id, message: error.message.slice(0, 300) }); }
        catch (sendError) { this.fail(sendError as Error); }
      });
      socket.on('close', () => {
        this.sockets.delete(id);
        this.paused.delete(socket);
        try { this.send({ type: 'close', id }); }
        catch (error) { this.fail(error as Error); }
      });
      return;
    }
    const socket = this.sockets.get(message.id);
    if (message.type === 'close' || message.type === 'error') { socket?.destroy(); return; }
    if (message.type !== 'data' || !socket) throw new Error('unexpected proxy message');
    const data = decodeProxyChunk(message.data);
    if (socket.writableLength + data.length > MAX_BUFFER_BYTES) throw new Error('repository write backpressure limit exceeded');
    socket.write(data);
  }

  private fail(error: Error): void {
    this.failure ??= error;
    this.rejectReady(error);
    this.close();
  }

  close(): void {
    if (this.intentionalClose) return;
    this.intentionalClose = true;
    clearInterval(this.resumeTimer);
    clearTimeout(this.lifetimeTimer);
    this.signal?.removeEventListener('abort', this.abort);
    for (const socket of this.sockets.values()) socket.destroy();
    this.sockets.clear();
    this.paused.clear();
    this.websocket.close();
  }
}
