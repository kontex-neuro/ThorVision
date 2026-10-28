// Gemini through its native ACP mode (`gemini --acp`), pinned in package.json.
import { spawn, type ChildProcess } from 'node:child_process';
import { existsSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from 'node:fs';
import { homedir, tmpdir } from 'node:os';
import { join } from 'node:path';
import { Readable, Writable } from 'node:stream';
import { client, methods, ndJsonStream, type ClientConnection, type NewSessionRequest, type SessionNotification } from '@agentclientprotocol/sdk';
import {
  subscriptionEnvironment, type AccountStatus, type AgentAdapter, type AgentEvent, type AgentSession,
  type Discovery, type LoginInfo, type PermissionAsker, type SessionOptions,
} from '../types.js';
import { INSTRUCTIONS, packageBin, packageVersion, toolsServer } from './common.js';

const PROTOCOL_VERSION = 1;
// Built-in Gemini tools that read, write, run or fetch outside the ThorVision tools.
export const EXCLUDED_TOOLS = [
  'run_shell_command', 'read_file', 'write_file', 'replace', 'glob', 'list_directory', 'read_many_files',
  'search_file_content', 'grep_search', 'web_fetch', 'google_web_search', 'save_memory', 'write_todos',
];
const SUBSCRIPTION_REMOVE = ['GEMINI_API_KEY', 'GOOGLE_API_KEY', 'GOOGLE_GENAI_USE_VERTEXAI', 'GOOGLE_GENAI_USE_GCA', 'GOOGLE_APPLICATION_CREDENTIALS'];
const SUBSCRIPTION_AUTH = 'oauth-personal';

/** System settings take precedence over the user's: tools are restricted and Google sign-in is enforced. */
export function systemSettings(directory: string): string {
  const path = join(directory, 'gemini-system-settings.json');
  writeFileSync(path, JSON.stringify({
    tools: { exclude: EXCLUDED_TOOLS },
    mcp: { allowed: ['thorvision'] },
    security: { auth: { selectedType: SUBSCRIPTION_AUTH, enforcedType: SUBSCRIPTION_AUTH } },
  }, null, 2));
  return path;
}

export function isAuthRequired(error: unknown): boolean {
  const code = (error as any)?.code;
  const message = String((error as any)?.message ?? '');
  return code === -32000 || /auth(entication)? required/i.test(message);
}

export function isQuotaError(error: unknown): boolean {
  return /quota|resource.?exhausted|429|rate.?limit/i.test(String((error as any)?.message ?? ''));
}

export interface GeminiOptions {
  /** Process launcher; tests substitute an ACP fixture agent. */
  launch?: (args: string[], env: Record<string, string>) => ChildProcess;
}

interface Connection {
  child: ChildProcess;
  connection: ClientConnection;
  updates: Map<string, (notification: SessionNotification) => void>;
  permissions: Map<string, PermissionAsker>;
  authMethods: { id: string; name: string }[];
}

export class GeminiAdapter implements AgentAdapter {
  readonly id = 'gemini' as const;
  readonly name = 'Gemini';
  private readonly runtime = mkdtempSync(join(tmpdir(), 'thorvision-gemini-'));
  private current: Connection | null = null;
  private readonly sessions = new Set<GeminiSession>();

  constructor(private readonly options: GeminiOptions = {}) {}

  async discover(): Promise<Discovery> {
    try {
      return { id: this.id, name: this.name, available: true, version: packageVersion('@google/gemini-cli'), runtime: 'ACP (stdio)', problems: [] };
    } catch (error) {
      return { id: this.id, name: this.name, available: false, version: null, runtime: 'ACP (stdio)', problems: [(error as Error).message] };
    }
  }

  private async connection(): Promise<Connection> {
    if (this.current && this.current.child.exitCode === null) return this.current;
    const env = subscriptionEnvironment(SUBSCRIPTION_REMOVE, { GEMINI_CLI_SYSTEM_SETTINGS_PATH: systemSettings(this.runtime) });
    const args = ['--acp', '--allowed-mcp-server-names', 'thorvision'];
    const child = this.options.launch
      ? this.options.launch(args, env)
      : spawn(process.execPath, [packageBin('@google/gemini-cli', 'gemini'), ...args], { env, stdio: ['pipe', 'pipe', 'pipe'], windowsHide: true });
    child.stderr?.on('data', (chunk) => process.stderr.write(`[gemini] ${chunk}`));
    const updates = new Map<string, (notification: SessionNotification) => void>();
    const permissions = new Map<string, PermissionAsker>();
    const app = client({ name: 'thorvision' })
      .onNotification(methods.client.session.update, ({ params }) => updates.get(params.sessionId)?.(params))
      .onRequest(methods.client.session.requestPermission, async ({ params }) => {
        const ask = permissions.get(params.sessionId);
        const allowed = ask ? await ask({ tool: params.toolCall.title ?? 'tool', title: params.toolCall.title ?? 'Tool request', input: params.toolCall.rawInput }) : false;
        const option = params.options.find((item) => item.kind === (allowed ? 'allow_once' : 'reject_once'))
          ?? params.options.find((item) => item.kind.startsWith(allowed ? 'allow' : 'reject'));
        return { outcome: option ? { outcome: 'selected', optionId: option.optionId } : { outcome: 'cancelled' } };
      });
    const stream = ndJsonStream(Writable.toWeb(child.stdin!) as WritableStream<Uint8Array>, Readable.toWeb(child.stdout!) as ReadableStream<Uint8Array>);
    const connection = app.connect(stream);
    child.on('exit', (code) => {
      connection.close(new Error(`gemini exited (${code})`));
      for (const session of this.sessions) session.processExited(code);
    });
    const reply = await connection.agent.request(methods.agent.initialize, {
      protocolVersion: PROTOCOL_VERSION,
      // No file system or terminal capabilities: the agent works only through the ThorVision tools.
      clientCapabilities: { fs: { readTextFile: false, writeTextFile: false }, terminal: false },
      clientInfo: { name: 'thorvision', version: '0.1.0' },
    });
    this.current = { child, connection, updates, permissions, authMethods: (reply.authMethods ?? []).map((method: any) => ({ id: method.id, name: method.name })) };
    return this.current;
  }

  async connect(): Promise<AccountStatus> {
    return this.accountStatus();
  }

  /** A session without prompts tells whether the cached Google sign-in is usable; the model is not called. */
  async accountStatus(): Promise<AccountStatus> {
    const billing = this.selectedAuth() === SUBSCRIPTION_AUTH ? 'Google account (Code Assist)' : 'Unknown';
    try {
      const current = await this.connection();
      const probe = mkdtempSync(join(this.runtime, 'probe-'));
      await current.connection.agent.request(methods.agent.session.new, { cwd: probe, mcpServers: [] });
      return { agent: this.id, state: 'connected', billing, account: null, detail: 'Signed in with Google' };
    } catch (error) {
      if (isAuthRequired(error)) return { agent: this.id, state: 'signin_required', billing, account: null, detail: 'Sign in with Google to use Gemini.' };
      if (isQuotaError(error)) return { agent: this.id, state: 'quota_exhausted', billing, account: null, detail: (error as Error).message };
      return { agent: this.id, state: 'error', billing, account: null, detail: (error as Error).message };
    }
  }

  private selectedAuth(): string | null {
    const path = join(homedir(), '.gemini', 'settings.json');
    try {
      return existsSync(path) ? JSON.parse(readFileSync(path, 'utf8'))?.security?.auth?.selectedType ?? null : null;
    } catch {
      return null;
    }
  }

  async login(): Promise<LoginInfo> {
    // Gemini's own Google sign-in opens the browser from the CLI.
    const current = await this.connection();
    if (!current.authMethods.some((method) => method.id === SUBSCRIPTION_AUTH)) {
      return { instructions: 'This Gemini build does not offer Google account sign-in over ACP.' };
    }
    await current.connection.agent.request(methods.agent.authenticate, { methodId: SUBSCRIPTION_AUTH });
    return { instructions: 'Gemini opened the Google sign-in in your browser; refresh the status when it completes.' };
  }

  async createSession(options: SessionOptions, emit: (event: AgentEvent) => void, ask: PermissionAsker): Promise<AgentSession> {
    const current = await this.connection();
    mkdirSync(options.workspace, { recursive: true });
    const server = toolsServer(options);
    const request: NewSessionRequest = {
      cwd: options.workspace,
      mcpServers: [{ name: 'thorvision', command: server.command, args: server.args,
        env: Object.entries(server.env).map(([name, value]) => ({ name, value })) }],
    };
    const reply = await current.connection.agent.request(methods.agent.session.new, request);
    const session = new GeminiSession(reply.sessionId, current, emit, () => {
      current.updates.delete(reply.sessionId);
      current.permissions.delete(reply.sessionId);
      this.sessions.delete(session);
    });
    current.updates.set(session.id, (notification) => session.update(notification));
    current.permissions.set(session.id, ask);
    this.sessions.add(session);
    return session;
  }

  async disconnect(): Promise<void> {
    for (const session of [...this.sessions]) await session.close();
    if (this.current?.child.exitCode === null) this.current.child.kill();
    this.current = null;
  }
}

class GeminiSession implements AgentSession {
  private primed = false;
  private closed = false;

  constructor(readonly id: string, private readonly current: Connection, private readonly emit: (event: AgentEvent) => void,
    private readonly forget: () => void) {}

  update(notification: SessionNotification): void {
    const update: any = notification.update;
    switch (update.sessionUpdate) {
      case 'agent_message_chunk':
        if (update.content?.type === 'text') this.emit({ type: 'message', text: update.content.text });
        break;
      case 'agent_thought_chunk':
        this.emit({ type: 'progress', text: 'Thinking' });
        break;
      case 'tool_call':
        this.emit({ type: 'toolStarted', callId: update.toolCallId, tool: update.title ?? 'tool', input: update.rawInput });
        break;
      case 'tool_call_update':
        if (update.status === 'completed' || update.status === 'failed') {
          this.emit({ type: 'toolFinished', callId: update.toolCallId, tool: update.title ?? 'tool', ok: update.status === 'completed' });
        }
        break;
      case 'plan':
        this.emit({ type: 'progress', text: (update.entries ?? []).map((entry: any) => entry.content).join('; ') });
        break;
    }
  }

  async sendPrompt(text: string): Promise<{ stopReason: string }> {
    // ACP has no system prompt; the instructions lead the first prompt of the session.
    const prompt = this.primed ? text : `${INSTRUCTIONS}\n\n${text}`;
    this.primed = true;
    try {
      const reply = await this.current.connection.agent.request(methods.agent.session.prompt, {
        sessionId: this.id, prompt: [{ type: 'text', text: prompt }],
      });
      this.emit({ type: 'completed', stopReason: reply.stopReason });
      return { stopReason: reply.stopReason };
    } catch (error) {
      const state = isAuthRequired(error) ? 'signin_required' : isQuotaError(error) ? 'quota_exhausted' : 'agent_error';
      this.emit({ type: 'failed', state, message: (error as Error).message });
      this.emit({ type: 'completed', stopReason: 'failed' });
      return { stopReason: 'failed' };
    }
  }

  async cancel(): Promise<void> {
    await this.current.connection.agent.notify(methods.agent.session.cancel, { sessionId: this.id }).catch(() => {});
  }

  processExited(code: number | null): void {
    if (!this.closed) this.emit({ type: 'failed', state: 'error', message: `Gemini exited (${code})` });
  }

  async close(): Promise<void> {
    if (this.closed) return;
    this.closed = true;
    await this.cancel();
    this.forget();
  }
}
