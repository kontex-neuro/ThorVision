// Newline-delimited JSON-RPC peer, used for the ThorVision client link and the Codex app-server.
import { createInterface } from 'node:readline';
import type { Readable, Writable } from 'node:stream';

export type Id = string | number;
type Handler = (method: string, params: any) => Promise<unknown>;
type Listener = (method: string, params: any) => void;

export class RpcError extends Error {
  constructor(readonly code: number, message: string, readonly data?: unknown) {
    super(message);
  }
}

export class JsonRpcPeer {
  private nextId = 1;
  private readonly pending = new Map<Id, { resolve(value: unknown): void; reject(error: Error): void; timer?: NodeJS.Timeout }>();
  private requestHandler: Handler = async (method) => {
    throw new RpcError(-32601, `method not found: ${method}`);
  };
  private notificationListener: Listener = () => {};
  private closedListener: (reason: string) => void = () => {};
  private closed = false;

  /** `version`: include `"jsonrpc": "2.0"` (the Codex app-server omits it). */
  constructor(input: Readable, private readonly output: Writable, private readonly version = true, private readonly maxLine = 8 << 20) {
    const lines = createInterface({ input, crlfDelay: Infinity });
    lines.on('line', (line) => this.receive(line));
    lines.on('close', () => this.close('stream closed'));
  }

  onRequest(handler: Handler): this {
    this.requestHandler = handler;
    return this;
  }

  onNotification(listener: Listener): this {
    this.notificationListener = listener;
    return this;
  }

  onClose(listener: (reason: string) => void): this {
    this.closedListener = listener;
    return this;
  }

  request<T = any>(method: string, params?: unknown, timeoutMs = 0): Promise<T> {
    if (this.closed) return Promise.reject(new RpcError(-32000, 'connection closed'));
    const id = this.nextId++;
    return new Promise<T>((resolve, reject) => {
      const entry: { resolve(value: unknown): void; reject(error: Error): void; timer?: NodeJS.Timeout } = {
        resolve: resolve as (value: unknown) => void,
        reject,
      };
      if (timeoutMs > 0) {
        entry.timer = setTimeout(() => {
          this.pending.delete(id);
          reject(new RpcError(-32001, `${method} timed out after ${timeoutMs} ms`));
        }, timeoutMs);
      }
      this.pending.set(id, entry);
      this.send({ id, method, ...(params === undefined ? {} : { params }) });
    });
  }

  notify(method: string, params?: unknown): void {
    if (!this.closed) this.send({ method, ...(params === undefined ? {} : { params }) });
  }

  close(reason = 'closed'): void {
    if (this.closed) return;
    this.closed = true;
    for (const [, entry] of this.pending) {
      if (entry.timer) clearTimeout(entry.timer);
      entry.reject(new RpcError(-32000, `connection closed: ${reason}`));
    }
    this.pending.clear();
    this.closedListener(reason);
  }

  private send(message: Record<string, unknown>): void {
    this.output.write(JSON.stringify(this.version ? { jsonrpc: '2.0', ...message } : message) + '\n');
  }

  private receive(line: string): void {
    if (!line.trim()) return;
    if (line.length > this.maxLine) {
      this.send({ id: null, error: { code: -32600, message: 'message too large' } });
      return;
    }
    let message: any;
    try {
      message = JSON.parse(line);
    } catch {
      this.send({ id: null, error: { code: -32700, message: 'parse error' } });
      return;
    }
    if (message && typeof message.method === 'string') {
      if (message.id === undefined || message.id === null) {
        try {
          this.notificationListener(message.method, message.params);
        } catch (error) {
          process.stderr.write(`notification ${message.method} failed: ${String(error)}\n`);
        }
        return;
      }
      this.requestHandler(message.method, message.params).then(
        (result) => this.send({ id: message.id, result: result ?? null }),
        (error: unknown) => {
          const rpc = error instanceof RpcError ? error : new RpcError(-32603, error instanceof Error ? error.message : String(error));
          this.send({ id: message.id, error: { code: rpc.code, message: rpc.message, ...(rpc.data === undefined ? {} : { data: rpc.data }) } });
        },
      );
      return;
    }
    const entry = this.pending.get(message?.id);
    if (!entry) return;
    this.pending.delete(message.id);
    if (entry.timer) clearTimeout(entry.timer);
    if (message.error) entry.reject(new RpcError(message.error.code ?? -32603, message.error.message ?? 'error', message.error.data));
    else entry.resolve(message.result);
  }
}
