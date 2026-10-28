// ThorVision agent bridge: launched by the client with QProcess; versioned JSON-RPC over stdio.
// Diagnostics go to stderr only; stdout carries protocol messages.
import { randomUUID } from 'node:crypto';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { JsonRpcPeer, RpcError } from './jsonrpc.js';
import { ClaudeAdapter } from './adapters/claude.js';
import { CodexAdapter } from './adapters/codex.js';
import { GeminiAdapter } from './adapters/gemini.js';
import { SDK_SNAPSHOT, TOOL_NAMES } from './adapters/common.js';
import type { AgentAdapter, AgentEvent, AgentId, AgentSession, PermissionAsker, PermissionRequest } from './types.js';

export const PROTOCOL_VERSION = 1;
const PERMISSION_TIMEOUT_MS = 5 * 60 * 1000;

/** The ThorVision tool a provider permission request refers to, if any. */
export function thorvisionTool(request: PermissionRequest): string | null {
  const text = `${request.tool} ${request.title}`;
  return TOOL_NAMES.find((name) => new RegExp(`(^|[^a-z_])(mcp__thorvision__)?${name}($|[^a-z_])`).test(text)) ?? null;
}

export class Bridge {
  private readonly adapters: Record<AgentId, AgentAdapter>;
  private readonly sessions = new Map<string, { agent: AgentId; session: AgentSession; approval: 'auto' | 'ask' }>();
  private readonly permissions = new Map<string, (allowed: boolean) => void>();

  constructor(private readonly peer: JsonRpcPeer, adapters?: Record<AgentId, AgentAdapter>) {
    this.adapters = adapters ?? { codex: new CodexAdapter(), claude: new ClaudeAdapter(), gemini: new GeminiAdapter() };
    peer.onRequest((method, params) => this.handle(method, params ?? {}));
    peer.onClose(() => void this.shutdown());
  }

  private adapter(id: unknown): AgentAdapter {
    const adapter = this.adapters[id as AgentId];
    if (!adapter) throw new RpcError(-32602, `unknown agent ${String(id)}`);
    return adapter;
  }

  private session(id: unknown) {
    const entry = this.sessions.get(String(id));
    if (!entry) throw new RpcError(-32602, `unknown session ${String(id)}`);
    return entry;
  }

  private asker(sessionId: string, emit: (event: AgentEvent) => void): PermissionAsker {
    return async (request) => {
      const tool = thorvisionTool(request);
      if (!tool) {
        emit({ type: 'progress', text: `Denied ${request.tool}: only ThorVision tools are available.` });
        return false;
      }
      if (this.sessions.get(sessionId)?.approval !== 'ask') return true;
      const requestId = randomUUID();
      emit({ type: 'permissionRequested', requestId, tool, title: request.title, input: request.input });
      return await new Promise<boolean>((resolve) => {
        const timer = setTimeout(() => {
          this.permissions.delete(requestId);
          resolve(false);
        }, PERMISSION_TIMEOUT_MS);
        this.permissions.set(requestId, (allowed) => {
          clearTimeout(timer);
          resolve(allowed);
        });
      });
    };
  }

  async handle(method: string, params: any): Promise<unknown> {
    switch (method) {
      case 'bridge.initialize': {
        if (params.protocolVersion !== PROTOCOL_VERSION) throw new RpcError(-32602, `bridge protocol ${PROTOCOL_VERSION} required`);
        const snapshot = JSON.parse(readFileSync(join(SDK_SNAPSHOT, 'SNAPSHOT.json'), 'utf8'));
        return { protocolVersion: PROTOCOL_VERSION, bridgeVersion: '0.1.0', agents: Object.keys(this.adapters), sdk: snapshot };
      }
      case 'agents.discover':
        return Promise.all(Object.values(this.adapters).map((adapter) => adapter.discover()));
      case 'agent.connect':
        return this.adapter(params.agent).connect();
      case 'agent.accountStatus':
        return this.adapter(params.agent).accountStatus();
      case 'agent.login': {
        const adapter = this.adapter(params.agent);
        const info = await adapter.login();
        if (adapter instanceof CodexAdapter) {
          void adapter.waitForLogin().then(async () => this.peer.notify('agent.status', await adapter.accountStatus()));
        }
        return info;
      }
      case 'agent.disconnect': {
        for (const [id, entry] of this.sessions) if (entry.agent === params.agent) this.sessions.delete(id);
        await this.adapter(params.agent).disconnect();
        return null;
      }
      case 'session.create': {
        const adapter = this.adapter(params.agent);
        for (const key of ['camera', 'workspace', 'extensionsUrl']) {
          if (typeof params[key] !== 'string' || !params[key]) throw new RpcError(-32602, `${key} is required`);
        }
        let sessionId = '';
        const emit = (event: AgentEvent) => this.peer.notify('session.event', { sessionId, agent: params.agent, ...event });
        const pending = randomUUID();
        sessionId = pending;
        this.sessions.set(pending, { agent: params.agent, session: null as unknown as AgentSession, approval: params.approval === 'ask' ? 'ask' : 'auto' });
        try {
          const session = await adapter.createSession({ camera: params.camera, workspace: params.workspace, extensionsUrl: params.extensionsUrl },
            emit, this.asker(pending, emit));
          this.sessions.get(pending)!.session = session;
        } catch (error) {
          this.sessions.delete(pending);
          throw error;
        }
        return { sessionId };
      }
      case 'session.prompt': {
        const entry = this.session(params.sessionId);
        if (typeof params.text !== 'string' || !params.text.trim()) throw new RpcError(-32602, 'text is required');
        return entry.session.sendPrompt(params.text);
      }
      case 'session.cancel':
        await this.session(params.sessionId).session.cancel();
        return null;
      case 'session.close': {
        const entry = this.session(params.sessionId);
        this.sessions.delete(String(params.sessionId));
        await entry.session.close();
        return null;
      }
      case 'permission.respond': {
        const resolve = this.permissions.get(String(params.requestId));
        if (!resolve) throw new RpcError(-32602, 'unknown or expired permission request');
        this.permissions.delete(String(params.requestId));
        resolve(params.allow === true);
        return null;
      }
      default:
        throw new RpcError(-32601, `method not found: ${method}`);
    }
  }

  async shutdown(): Promise<void> {
    for (const resolve of this.permissions.values()) resolve(false);
    this.permissions.clear();
    for (const entry of this.sessions.values()) await entry.session?.close().catch(() => {});
    this.sessions.clear();
    for (const adapter of Object.values(this.adapters)) await adapter.disconnect().catch(() => {});
  }
}

if (process.argv[1] && import.meta.filename.endsWith(process.argv[1].split(/[\\/]/).pop()!)) {
  const peer = new JsonRpcPeer(process.stdin, process.stdout);
  const bridge = new Bridge(peer);
  peer.onClose(() => {
    void bridge.shutdown().finally(() => process.exit(0));
  });
  for (const signal of ['SIGINT', 'SIGTERM'] as const) process.on(signal, () => void bridge.shutdown().finally(() => process.exit(0)));
}
