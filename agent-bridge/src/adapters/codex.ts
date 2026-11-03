// Codex through its native app-server protocol (JSON-RPC over stdio), pinned in package.json.
import { spawn, type ChildProcess } from 'node:child_process';
import { JsonRpcPeer, RpcError } from '../jsonrpc.js';
import type { GetAccountResponse } from '../protocol/codex/v2/GetAccountResponse.js';
import type { LoginAccountResponse } from '../protocol/codex/v2/LoginAccountResponse.js';
import type { ThreadStartResponse } from '../protocol/codex/v2/ThreadStartResponse.js';
import type { TurnStartResponse } from '../protocol/codex/v2/TurnStartResponse.js';
import type { TurnCompletedNotification } from '../protocol/codex/v2/TurnCompletedNotification.js';
import type { ErrorNotification } from '../protocol/codex/v2/ErrorNotification.js';
import type { CodexErrorInfo } from '../protocol/codex/v2/CodexErrorInfo.js';
import type { ThreadItem } from '../protocol/codex/v2/ThreadItem.js';
import {
  subscriptionEnvironment, type AccountStatus, type AgentAdapter, type AgentEvent, type AgentSession,
  type ConnectionState, type Discovery, type LoginInfo, type PermissionAsker, type SessionOptions,
} from '../types.js';
import { INSTRUCTIONS, packageBin, packageVersion, toolsServer } from './common.js';

// Native shell, exec, browser, computer use, apps, plugins, image generation and web search are off:
// the agent works only through the ThorVision tools.
export const LOCKDOWN = [
  '--disable', 'shell_tool', '--disable', 'unified_exec', '--disable', 'apps', '--disable', 'plugins',
  '--disable', 'browser_use', '--disable', 'computer_use', '--disable', 'in_app_browser',
  '--disable', 'image_generation', '-c', 'web_search="disabled"',
];
const SUBSCRIPTION_REMOVE = ['OPENAI_API_KEY', 'CODEX_API_KEY', 'OPENAI_BASE_URL', 'OPENAI_ORG_ID', 'OPENAI_PROJECT_ID'];

/** TOML for `-c key=value`: JSON strings and arrays are valid TOML; objects become inline tables. */
export function toml(value: unknown): string {
  if (value && typeof value === 'object' && !Array.isArray(value)) {
    return '{ ' + Object.entries(value).map(([key, inner]) => `${key} = ${toml(inner)}`).join(', ') + ' }';
  }
  return JSON.stringify(value);
}

export function quotaState(info: CodexErrorInfo | null | undefined): ConnectionState | null {
  if (info === 'usageLimitExceeded' || info === 'sessionBudgetExceeded') return 'quota_exhausted';
  if (info === 'unauthorized') return 'signin_required';
  return null;
}

export interface CodexOptions {
  /** Process launcher; tests substitute a protocol fixture. */
  launch?: (args: string[], env: Record<string, string>) => ChildProcess;
}

/** One app-server process with the initialize handshake done. */
class CodexProcess {
  readonly peer: JsonRpcPeer;
  exited: ((code: number | null) => void) | null = null;

  constructor(readonly child: ChildProcess) {
    this.peer = new JsonRpcPeer(child.stdout!, child.stdin!, false);
    child.stderr?.on('data', (chunk) => process.stderr.write(`[codex] ${chunk}`));
    child.on('exit', (code) => {
      this.peer.close(`codex exited (${code})`);
      this.exited?.(code);
    });
  }

  async initialize(): Promise<void> {
    await this.peer.request('initialize', { clientInfo: { name: 'thorvision', title: 'ThorVision', version: '0.1.0' }, capabilities: null }, 30000);
    this.peer.notify('initialized');
  }

  stop(): void {
    if (this.child.exitCode === null) this.child.kill();
  }
}

export class CodexAdapter implements AgentAdapter {
  readonly id = 'codex' as const;
  readonly name = 'Codex';
  private account: CodexProcess | null = null;
  private readonly sessions = new Set<CodexSession>();
  private loginWaiters: ((success: boolean, error: string | null) => void)[] = [];

  constructor(private readonly options: CodexOptions = {}) {}

  async discover(): Promise<Discovery> {
    try {
      return { id: this.id, name: this.name, available: true, version: packageVersion('@openai/codex'), runtime: 'app-server (stdio)', problems: [] };
    } catch (error) {
      return { id: this.id, name: this.name, available: false, version: null, runtime: 'app-server (stdio)', problems: [(error as Error).message] };
    }
  }

  /** `servers` replaces the whole mcp_servers table, so servers from the user's Codex config never load here. */
  private launch(servers: Record<string, unknown> = {}): CodexProcess {
    const env = subscriptionEnvironment(SUBSCRIPTION_REMOVE);
    const args = ['app-server', ...LOCKDOWN, '-c', `mcp_servers=${toml(servers)}`];
    const child = this.options.launch
      ? this.options.launch(args, env)
      : spawn(process.execPath, [packageBin('@openai/codex', 'codex'), ...args], { env, stdio: ['pipe', 'pipe', 'pipe'], windowsHide: true });
    return new CodexProcess(child);
  }

  /** Account operations run on a process without ThorVision tools. */
  private async accountProcess(): Promise<CodexProcess> {
    if (this.account) return this.account;
    const process = this.launch();
    process.exited = () => {
      if (this.account === process) this.account = null;
    };
    process.peer.onNotification((method, params) => {
      if (method === 'account/login/completed') for (const waiter of this.loginWaiters.splice(0)) waiter(params.success, params.error);
    });
    this.account = process;
    await process.initialize();
    return process;
  }

  async connect(): Promise<AccountStatus> {
    return this.accountStatus();
  }

  async accountStatus(): Promise<AccountStatus> {
    try {
      const process = await this.accountProcess();
      return this.status(await process.peer.request<GetAccountResponse>('account/read', { refreshToken: false }, 20000));
    } catch (error) {
      return { agent: this.id, state: 'error', billing: 'Unknown', account: null, detail: (error as Error).message };
    }
  }

  status(reply: GetAccountResponse): AccountStatus {
    const account = reply.account;
    if (!account) {
      return { agent: this.id, state: reply.requiresOpenaiAuth ? 'signin_required' : 'unknown', billing: 'Unknown', account: null,
        detail: reply.requiresOpenaiAuth ? 'Sign in with ChatGPT to use Codex.' : 'Codex reported no account.' };
    }
    if (account.type === 'chatgpt') {
      return { agent: this.id, state: 'connected', billing: `ChatGPT ${account.planType}`, account: account.email, detail: 'Subscription sign-in' };
    }
    // Subscription mode never uses API-key or cloud billing, even when it is configured.
    return { agent: this.id, state: 'unsupported', billing: account.type === 'apiKey' ? 'API key' : account.type, account: null,
      detail: 'Codex is configured for API or cloud billing; sign in with ChatGPT for subscription use.' };
  }

  async login(): Promise<LoginInfo> {
    const process = await this.accountProcess();
    const reply = await process.peer.request<LoginAccountResponse>('account/login/start', { type: 'chatgpt' }, 30000);
    if (reply.type === 'chatgpt') {
      return { url: reply.authUrl, instructions: 'Complete the ChatGPT sign-in in your browser; ThorVision refreshes the status when it finishes.' };
    }
    return { instructions: 'Codex did not offer a browser sign-in.' };
  }

  waitForLogin(): Promise<{ success: boolean; error: string | null }> {
    return new Promise((resolve) => this.loginWaiters.push((success, error) => resolve({ success, error })));
  }

  async createSession(options: SessionOptions, emit: (event: AgentEvent) => void, ask: PermissionAsker): Promise<AgentSession> {
    const server = toolsServer(options);
    const process = this.launch({ thorvision: { command: server.command, args: server.args, env: server.env, tool_timeout_sec: 900 } });
    const session = new CodexSession(process, emit, ask);
    process.exited = (code) => {
      this.sessions.delete(session);
      session.processExited(code);
    };
    try {
      await process.initialize();
      const reply = await process.peer.request<ThreadStartResponse>('thread/start', {
        cwd: options.workspace, sandbox: 'read-only', approvalPolicy: 'untrusted',
        developerInstructions: INSTRUCTIONS, serviceName: 'thorvision',
      }, 60000);
      session.id = reply.thread.id;
    } catch (error) {
      process.stop();
      throw error;
    }
    this.sessions.add(session);
    return session;
  }

  async disconnect(): Promise<void> {
    this.account?.stop();
    this.account = null;
    for (const session of this.sessions) await session.close();
    this.sessions.clear();
  }
}

class CodexSession implements AgentSession {
  id = '';
  private turnId: string | null = null;
  private finish: ((stopReason: string) => void) | null = null;
  private closed = false;

  constructor(private readonly process: CodexProcess, private readonly emit: (event: AgentEvent) => void,
    private readonly ask: PermissionAsker) {
    process.peer.onNotification((method, params) => this.notification(method, params));
    process.peer.onRequest((method, params) => this.serverRequest(method, params));
  }

  async sendPrompt(text: string): Promise<{ stopReason: string }> {
    const done = new Promise<string>((resolve) => (this.finish = resolve));
    const reply = await this.process.peer.request<TurnStartResponse>('turn/start', {
      threadId: this.id, input: [{ type: 'text', text, text_elements: [] }],
    }, 60000);
    this.turnId = reply.turn.id;
    return { stopReason: await done };
  }

  async cancel(): Promise<void> {
    if (this.turnId) await this.process.peer.request('turn/interrupt', { threadId: this.id, turnId: this.turnId }, 15000).catch(() => {});
  }

  async close(): Promise<void> {
    if (this.closed) return;
    this.closed = true;
    await this.cancel();
    this.process.stop();
  }

  processExited(code: number | null): void {
    if (!this.closed) this.emit({ type: 'failed', state: 'error', message: `Codex exited (${code})` });
    this.finish?.('failed');
    this.finish = null;
  }

  notification(method: string, params: any): void {
    switch (method) {
      case 'item/agentMessage/delta':
        this.emit({ type: 'message', text: params.delta });
        break;
      case 'item/started':
      case 'item/completed': {
        const item = params.item as ThreadItem;
        if (item.type === 'mcpToolCall') {
          if (method === 'item/started') this.emit({ type: 'toolStarted', callId: item.id, tool: item.tool, input: item.arguments });
          else this.emit({ type: 'toolFinished', callId: item.id, tool: item.tool, ok: item.status === 'completed' });
        } else if (item.type === 'reasoning' && method === 'item/started') {
          this.emit({ type: 'progress', text: 'Thinking' });
        }
        break;
      }
      case 'error': {
        const notification = params as ErrorNotification;
        if (notification.willRetry) this.emit({ type: 'progress', text: `Retrying: ${notification.error.message}` });
        else this.emit({ type: 'failed', state: quotaState(notification.error.codexErrorInfo) ?? 'agent_error', message: notification.error.message });
        break;
      }
      case 'turn/completed': {
        const turn = (params as TurnCompletedNotification).turn;
        if (turn.status === 'failed' && turn.error) {
          this.emit({ type: 'failed', state: quotaState(turn.error.codexErrorInfo) ?? 'agent_error', message: turn.error.message });
        }
        this.emit({ type: 'completed', stopReason: turn.status });
        this.finish?.(turn.status);
        this.finish = null;
        this.turnId = null;
        break;
      }
    }
  }

  async serverRequest(method: string, params: any): Promise<unknown> {
    switch (method) {
      // Native commands and file changes are never part of this workflow.
      case 'item/commandExecution/requestApproval':
      case 'item/fileChange/requestApproval':
        this.emit({ type: 'progress', text: 'Declined a native command or file change; only ThorVision tools are allowed.' });
        return { decision: 'decline' };
      case 'applyPatchApproval':
      case 'execCommandApproval':
        return { decision: { denied: { rejection: 'Only ThorVision tools are allowed in this workflow.' } } };
      case 'mcpServer/elicitation/request': {
        const allowed = await this.ask({ tool: params.serverName ?? 'thorvision', title: params.message ?? 'Tool request', input: params });
        return { action: allowed ? 'accept' : 'decline', content: null, _meta: null };
      }
      case 'item/permissions/requestApproval':
        this.emit({ type: 'progress', text: 'Declined an additional sandbox permission request.' });
        return { permissions: {}, scope: 'turn' };
      case 'item/tool/requestUserInput':
        return { answers: {} };
      default:
        throw new RpcError(-32601, `ThorVision does not handle ${method}`);
    }
  }
}
