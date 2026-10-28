// Claude through the official TypeScript Agent SDK and the local Claude Code sign-in.
import { randomUUID } from 'node:crypto';
import { query as sdkQuery, type AccountInfo, type Options, type Query, type SDKMessage, type SDKUserMessage } from '@anthropic-ai/claude-agent-sdk';
import {
  subscriptionEnvironment, type AccountStatus, type AgentAdapter, type AgentEvent, type AgentSession,
  type ConnectionState, type Discovery, type LoginInfo, type PermissionAsker, type SessionOptions,
} from '../types.js';
import { INSTRUCTIONS, packageVersion, toolsServer } from './common.js';

// In subscription mode the Claude Code login is used; API keys and cloud-provider routing are removed.
const SUBSCRIPTION_REMOVE = [
  'ANTHROPIC_API_KEY', 'ANTHROPIC_AUTH_TOKEN', 'ANTHROPIC_BASE_URL', 'CLAUDE_CODE_USE_BEDROCK',
  'CLAUDE_CODE_USE_VERTEX', 'CLAUDE_CODE_USE_FOUNDRY',
];

export type QueryFunction = (params: { prompt: string | AsyncIterable<SDKUserMessage>; options?: Options }) => Query;

export function assistantErrorState(error: string | undefined): ConnectionState | 'agent_error' {
  switch (error) {
    case 'authentication_failed':
    case 'oauth_org_not_allowed':
    case 'verification_required':
      return 'signin_required';
    case 'billing_error':
    case 'rate_limit':
      return 'quota_exhausted';
    case 'account_on_hold':
      return 'unsupported';
    default:
      return 'agent_error';
  }
}

export function accountStatus(info: AccountInfo): AccountStatus {
  const base = { agent: 'claude' as const, account: info.email ?? null };
  if (info.apiProvider && info.apiProvider !== 'firstParty') {
    return { ...base, state: 'unsupported', billing: info.apiProvider, detail: 'Claude Code is routed to a cloud provider; subscription mode needs a Claude login.' };
  }
  if (info.subscriptionType) {
    const plan = /^claude\b/i.test(info.subscriptionType) ? info.subscriptionType : `Claude ${info.subscriptionType}`;
    return { ...base, state: 'connected', billing: plan, detail: info.tokenSource ? `Signed in (${info.tokenSource})` : 'Signed in' };
  }
  if (info.apiKeySource && info.apiKeySource !== 'none' && info.apiKeySource !== 'oauth') {
    return { ...base, state: 'unsupported', billing: 'API key', detail: 'Only an API key is configured; subscription mode does not use API billing.' };
  }
  if (!info.email && (!info.tokenSource || info.tokenSource === 'none')) {
    return { ...base, state: 'signin_required', billing: 'Unknown', detail: 'Sign in to Claude Code to use Claude.' };
  }
  return { ...base, state: 'connected', billing: 'Unknown', detail: 'Signed in; the plan is not reported.' };
}

/** Async input queue feeding a streaming-input query; `end` finishes the iteration. */
class Inbox implements AsyncIterable<SDKUserMessage> {
  private readonly items: SDKUserMessage[] = [];
  private waiter: ((result: IteratorResult<SDKUserMessage>) => void) | null = null;
  private ended = false;

  push(text: string): void {
    const message: SDKUserMessage = { type: 'user', message: { role: 'user', content: text }, parent_tool_use_id: null };
    if (this.waiter) {
      this.waiter({ value: message, done: false });
      this.waiter = null;
    } else this.items.push(message);
  }

  end(): void {
    this.ended = true;
    this.waiter?.({ value: undefined, done: true });
    this.waiter = null;
  }

  [Symbol.asyncIterator](): AsyncIterator<SDKUserMessage> {
    return {
      next: () => {
        const item = this.items.shift();
        if (item) return Promise.resolve({ value: item, done: false });
        if (this.ended) return Promise.resolve({ value: undefined, done: true });
        return new Promise((resolve) => (this.waiter = resolve));
      },
    };
  }
}

export class ClaudeAdapter implements AgentAdapter {
  readonly id = 'claude' as const;
  readonly name = 'Claude';
  private readonly sessions = new Set<ClaudeSession>();

  constructor(private readonly query: QueryFunction = sdkQuery) {}

  async discover(): Promise<Discovery> {
    try {
      return { id: this.id, name: this.name, available: true, version: packageVersion('@anthropic-ai/claude-agent-sdk'), runtime: 'Agent SDK', problems: [] };
    } catch (error) {
      return { id: this.id, name: this.name, available: false, version: null, runtime: 'Agent SDK', problems: [(error as Error).message] };
    }
  }

  private options(extra: Partial<Options>): Options {
    return {
      tools: [],
      settingSources: [],
      systemPrompt: INSTRUCTIONS,
      env: subscriptionEnvironment(SUBSCRIPTION_REMOVE, { CLAUDE_AGENT_SDK_CLIENT_APP: 'thorvision-agent-bridge/0.1.0' }),
      ...extra,
    };
  }

  async connect(): Promise<AccountStatus> {
    return this.accountStatus();
  }

  /** Reads the account of the local sign-in without sending a prompt. */
  async accountStatus(): Promise<AccountStatus> {
    const inbox = new Inbox();
    const run = this.query({ prompt: inbox, options: this.options({}) });
    try {
      return accountStatus(await withTimeout(run.accountInfo(), 30000, 'Claude Code did not report its account'));
    } catch (error) {
      return { agent: this.id, state: 'error', billing: 'Unknown', account: null, detail: (error as Error).message };
    } finally {
      inbox.end();
      await run.return(undefined).catch(() => {});
    }
  }

  async login(): Promise<LoginInfo> {
    return {
      terminal: { command: 'claude', args: ['/login'] },
      instructions: 'Sign in with the Claude Code login in the terminal that opens, then refresh the status in ThorVision.',
    };
  }

  async createSession(options: SessionOptions, emit: (event: AgentEvent) => void, ask: PermissionAsker): Promise<AgentSession> {
    const inbox = new Inbox();
    const server = toolsServer(options);
    const run = this.query({
      prompt: inbox,
      options: this.options({
        cwd: options.workspace,
        mcpServers: { thorvision: { type: 'stdio', command: server.command, args: server.args, env: server.env } },
        strictMcpConfig: true,
        canUseTool: async (toolName, input) => {
          const allowed = await ask({ tool: toolName, title: `Use ${toolName}`, input });
          return allowed ? { behavior: 'allow', updatedInput: input } : { behavior: 'deny', message: 'Denied by the ThorVision user or policy.' };
        },
      }),
    });
    const session = new ClaudeSession(run, inbox, emit, () => this.sessions.delete(session));
    this.sessions.add(session);
    return session;
  }

  async disconnect(): Promise<void> {
    for (const session of [...this.sessions]) await session.close();
  }
}

class ClaudeSession implements AgentSession {
  readonly id = randomUUID();
  private finish: ((stopReason: string) => void) | null = null;
  private readonly tools = new Map<string, string>();
  private closed = false;

  constructor(private readonly run: Query, private readonly inbox: Inbox, private readonly emit: (event: AgentEvent) => void,
    private readonly forget: () => void) {
    void this.pump();
  }

  private async pump(): Promise<void> {
    try {
      for await (const message of this.run) this.handle(message);
      if (!this.closed) this.emit({ type: 'failed', state: 'error', message: 'Claude Code ended the session' });
    } catch (error) {
      if (!this.closed) this.emit({ type: 'failed', state: 'error', message: (error as Error).message });
    } finally {
      this.finish?.('failed');
      this.finish = null;
      this.forget();
    }
  }

  handle(message: SDKMessage): void {
    switch (message.type) {
      case 'assistant': {
        if (message.error) {
          this.emit({ type: 'failed', state: assistantErrorState(message.error), message: `Claude reported ${message.error}` });
        }
        for (const block of message.message.content as any[]) {
          if (block.type === 'text' && block.text) this.emit({ type: 'message', text: block.text });
          if (block.type === 'tool_use') {
            this.tools.set(block.id, block.name);
            this.emit({ type: 'toolStarted', callId: block.id, tool: block.name, input: block.input });
          }
        }
        break;
      }
      case 'user': {
        const content = message.message.content;
        if (Array.isArray(content)) {
          for (const block of content as any[]) {
            if (block.type === 'tool_result') {
              this.emit({ type: 'toolFinished', callId: block.tool_use_id, tool: this.tools.get(block.tool_use_id) ?? 'tool', ok: !block.is_error });
            }
          }
        }
        break;
      }
      case 'auth_status':
        if (message.error) this.emit({ type: 'failed', state: 'signin_required', message: message.error });
        break;
      case 'result': {
        const stopReason = message.subtype === 'success' ? (message.stop_reason ?? 'end_turn') : message.subtype;
        if (message.subtype !== 'success') this.emit({ type: 'failed', state: 'agent_error', message: `Claude stopped: ${message.subtype}` });
        this.emit({ type: 'completed', stopReason });
        this.finish?.(stopReason);
        this.finish = null;
        break;
      }
    }
  }

  async sendPrompt(text: string): Promise<{ stopReason: string }> {
    const done = new Promise<string>((resolve) => (this.finish = resolve));
    this.inbox.push(text);
    return { stopReason: await done };
  }

  async cancel(): Promise<void> {
    await this.run.interrupt().catch(() => {});
  }

  async close(): Promise<void> {
    if (this.closed) return;
    this.closed = true;
    this.inbox.end();
    await this.run.return(undefined).catch(() => {});
  }
}

function withTimeout<T>(promise: Promise<T>, ms: number, message: string): Promise<T> {
  let timer: NodeJS.Timeout | undefined;
  const timeout = new Promise<T>((_, reject) => (timer = setTimeout(() => reject(new Error(message)), ms)));
  return Promise.race([promise, timeout]).finally(() => clearTimeout(timer));
}
