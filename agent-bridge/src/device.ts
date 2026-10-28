// Client for the device extension service, reached through the ThorVision SSH tunnel.

export class DeviceError extends Error {
  constructor(readonly status: number, message: string) {
    super(message);
  }
}

export class DeviceClient {
  constructor(private readonly baseUrl: string, private readonly timeoutMs = 15000) {
    if (!/^http:\/\/127\.0\.0\.1:\d+$/.test(baseUrl)) throw new Error('the extension service is only reached through the local tunnel');
  }

  async json(method: string, path: string, body?: Buffer | string, timeoutMs = this.timeoutMs): Promise<any> {
    let response: Response;
    try {
      response = await fetch(this.baseUrl + path, {
        method,
        body: body === undefined ? undefined : typeof body === 'string' ? body : new Uint8Array(body),
        headers: body === undefined ? undefined : { 'content-type': typeof body === 'string' ? 'application/json' : 'application/gzip' },
        signal: AbortSignal.timeout(timeoutMs),
      });
    } catch (error) {
      throw new DeviceError(0, `device extension service unreachable: ${(error as Error).message}`);
    }
    const text = await response.text();
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

  state() {
    return this.json('GET', '/v1/state');
  }

  upload(archive: Buffer) {
    return this.json('POST', '/v1/packages', archive, 60000);
  }

  packages() {
    return this.json('GET', '/v1/packages');
  }

  /** Starts a prepare/test job and waits for its report. */
  async job(kind: 'prepare' | 'test', digest: string, timeoutMs = 300000): Promise<any> {
    const job = await this.json('POST', `/v1/packages/${digest}/${kind}`, '');
    const deadline = Date.now() + timeoutMs;
    let current = job;
    while (current.state === 'running') {
      if (Date.now() > deadline) throw new DeviceError(0, `${kind} job ${job.id} did not finish within ${timeoutMs / 1000} s`);
      await new Promise((resolve) => setTimeout(resolve, 500));
      current = await this.json('GET', `/v1/jobs/${job.id}`);
    }
    return current;
  }

  /** One processed-preview message (TVP1 header + JPEG) for a camera, if analytics is running there. */
  async previewFrame(camera: string, timeoutMs = 4000): Promise<{ header: any; jpeg: Buffer } | null> {
    const url = this.baseUrl.replace('http://', 'ws://') + `/v1/cameras/${encodeURIComponent(camera)}/preview`;
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
