// Client for analytics through the pre-deployed device API; loopback APIs support local development.
import { validateDeviceUrl } from './device-url.js';
import { PreparationProxy } from './preparation-proxy.js';
import { digitalIOStatus, unsupportedDigitalIO, type DigitalIOStatus } from './digital-io-schema.js';

export class DeviceError extends Error {
  constructor(readonly status: number, message: string) {
    super(message);
  }
}

export class DeviceClient {
  get url(): string { return this.baseUrl; }
  constructor(private readonly baseUrl: string, private readonly timeoutMs = 15000) {
    validateDeviceUrl(baseUrl);
  }

  async json(method: string, path: string, body?: Buffer | string, timeoutMs = this.timeoutMs,
    headers: Record<string, string> = {}): Promise<any> {
    return this.request(this.baseUrl + path, method, body, timeoutMs, headers);
  }

  private async request(url: string, method: string, body: Buffer | string | undefined, timeoutMs: number,
    headers: Record<string, string> = {}): Promise<any> {
    let response: Response;
    try {
      response = await fetch(url, {
        method,
        body: body === undefined ? undefined : typeof body === 'string' ? body : new Uint8Array(body),
        headers: { ...headers, ...(body === undefined ? {} : { 'content-type': typeof body === 'string' ? 'application/json' : 'application/gzip' }) },
        redirect: 'error',
        signal: AbortSignal.timeout(timeoutMs),
      });
    } catch (error) {
      throw new DeviceError(0, `device extension service unreachable: ${(error as Error).message}`);
    }
    let text: string;
    try {
      text = await response.text();
    } catch (error) {
      throw new DeviceError(0, `device response body transport failed: ${error instanceof Error ? error.message : String(error)}`);
    }
    let value: any = null;
    try {
      value = text ? JSON.parse(text) : null;
    } catch {
      value = { error: text.slice(0, 500) };
    }
    if (!response.ok) throw new DeviceError(response.status, value?.error ?? `HTTP ${response.status}`);
    return value;
  }

  capabilities() {
    return this.json('GET', '/v1/capabilities');
  }

  /** Root sibling API on the already validated selected device; never a caller-supplied target. */
  async digitalIO(): Promise<DigitalIOStatus> {
    try {
      return digitalIOStatus.parse(await this.request(new URL('/digital-io', this.baseUrl).href,
        'GET', undefined, this.timeoutMs));
    } catch (error) {
      if (error instanceof DeviceError && error.status === 404) return unsupportedDigitalIO();
      throw error;
    }
  }

  async writeDigitalOutputs(body: { request_id: string; publisher_instance: string; expected_revision: string; mask: number; values: number }): Promise<unknown> {
    try {
      return await this.request(new URL('/digital-io/outputs', this.baseUrl).href, 'PUT', JSON.stringify(body), 1500);
    } catch (error) {
      if (error instanceof DeviceError && error.status === 0)
        throw new DeviceError(504, 'Digital I/O write transport failed; completion is uncertain. Do not resend automatically.');
      throw error;
    }
  }

  state() {
    return this.json('GET', '/v1/state');
  }

  upload(archive: Buffer) {
    return this.json('POST', '/v1/packages', archive, 60000);
  }

  packages() {
    return this.json('GET', '/v1/packages');
  }

  /** The existing camera API is a sibling of /analytics, not an analytics route. */
  async cameras(): Promise<any[]> {
    const base = new URL(this.baseUrl);
    if (!['/framelab', '/analytics'].includes(base.pathname.replace(/\/$/, ''))) return [];
    const result = await this.request(new URL('/cameras', base).href, 'GET', undefined, this.timeoutMs);
    if (!Array.isArray(result)) throw new DeviceError(0, 'device camera catalog is not an array');
    return result;
  }

  apply(camera: string, digest: string, config: Record<string, unknown>, revision: number, key: string) {
    return this.json('PUT', `/v1/cameras/${encodeURIComponent(camera)}/extension`,
      JSON.stringify({ digest, config, revision }), 75000, { 'Idempotency-Key': key });
  }

  stop(camera: string, revision: number) {
    return this.json('DELETE', `/v1/cameras/${encodeURIComponent(camera)}/extension?revision=${revision}`);
  }

  cleanupPlan(packages: string[], includeUnusedCache: boolean) {
    return this.json('POST', '/v1/cleanup/plan', JSON.stringify({ packages, include_unused_cache: includeUnusedCache }));
  }

  cleanupExecute(id: string) {
    return this.json('POST', '/v1/cleanup/execute', JSON.stringify({ id }), 75000);
  }

  /** Starts a prepare/test job and waits for its report. */
  async job(kind: 'prepare' | 'test', digest: string, timeoutMs = 300000, body?: unknown,
    progress: (message: string) => void = () => {}, signal?: AbortSignal): Promise<any> {
    const job = await this.json('POST', `/v1/packages/${digest}/${kind}`, body ? JSON.stringify(body) : '');
    const proxy = job.proxy_token ? new PreparationProxy(
      `${this.baseUrl.replace(/^http/, 'ws')}/v1/jobs/${job.id}/proxy?token=${job.proxy_token}`, progress, signal) : undefined;
    const deadline = Date.now() + timeoutMs;
    let current = job;
    try {
      await proxy?.connected();
      while (current.state === 'running') {
        signal?.throwIfAborted();
        proxy?.check();
        if (Date.now() > deadline) throw new DeviceError(0, `${kind} job ${job.id} did not finish within ${timeoutMs / 1000} s`);
        await new Promise((resolve) => setTimeout(resolve, 500));
        current = await this.json('GET', `/v1/jobs/${job.id}`);
        if (current.stage || current.progress) progress(String(current.stage || current.progress));
      }
      return current;
    } finally {
      proxy?.close();
    }
  }

  /** One processed-preview message (TVP1 header + JPEG) for a camera, if analytics is running there. */
  async previewFrame(camera: string, timeoutMs = 4000): Promise<{ header: any; jpeg: Buffer } | null> {
    const url = this.baseUrl.replace(/^http/, 'ws') + `/v1/cameras/${encodeURIComponent(camera)}/preview`;
    return await new Promise((resolve) => {
      const socket = new WebSocket(url);
      socket.binaryType = 'arraybuffer';
      const timer = setTimeout(() => {
        socket.close();
        resolve(null);
      }, timeoutMs);
      socket.onmessage = (event) => {
        const data = Buffer.from(event.data as ArrayBuffer);
        clearTimeout(timer);
        socket.close();
        if (data.subarray(0, 4).toString() !== 'TVP1') return resolve(null);
        const size = data.readUInt32LE(4);
        resolve({ header: JSON.parse(data.subarray(8, 8 + size).toString()), jpeg: data.subarray(8 + size) });
      };
      socket.onerror = () => {
        clearTimeout(timer);
        resolve(null);
      };
    });
  }
}
