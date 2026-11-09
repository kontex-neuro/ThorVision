// Protocol fixtures for every adapter: sign-in required, streamed messages, tool calls,
// permission routing, cancellation, quota exhaustion and process exit.
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { mkdtempSync, readFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { describe, it } from 'node:test';
import type { Options, Query, SDKMessage, SDKUserMessage } from '@anthropic-ai/claude-agent-sdk';
import { ClaudeAdapter, accountStatus as claudeStatus, type QueryFunction } from '../adapters/claude.js';
import { CodexAdapter, LOCKDOWN } from '../adapters/codex.js';
import { EXCLUDED_TOOLS, GeminiAdapter } from '../adapters/gemini.js';
import { thorvisionTool } from '../main.js';
import type { AgentEvent, PermissionAsker, SessionOptions } from '../types.js';

const FIXTURES = join(import.meta.dirname, 'fixtures');
const OPTIONS: SessionOptions = { camera: '/dev/v4l/by-id/cam', workspace: mkdtempSync(join(tmpdir(), 'tv-ws-')), extensionsUrl: 'http://127.0.0.1:1' };
const policy: PermissionAsker = async (request) => thorvisionTool(request) !== null;

function launcher(fixture: string, scenario: string, argsFile?: string) {
  return (args: string[], env: Record<string, string>) =>
    spawn(process.execPath, [join(FIXTURES, fixture), ...args], {
      env: { ...env, FAKE_SCENARIO: scenario, ...(argsFile ? { FAKE_ARGS_FILE: argsFile } : {}) },
      stdio: ['pipe', 'pipe', 'pipe'],
    });
}

function collector() {
  const events: AgentEvent[] = [];
  return { events, emit: (event: AgentEvent) => events.push(event), text: () => events.filter((e) => e.type === 'message').map((e: any) => e.text).join('') };
}

describe('Codex adapter (app-server protocol)', () => {
  it('reports sign-in, subscription and API-key billing as connection states', async () => {
    for (const [scenario, state, billing] of [['signin', 'signin_required', 'Unknown'], ['chatgpt', 'connected', 'ChatGPT plus'], ['apikey', 'unsupported', 'API key']]) {
      const adapter = new CodexAdapter({ launch: launcher('fake-codex.js', scenario) });
      const status = await adapter.accountStatus();
      assert.equal(status.state, state, scenario);
      assert.equal(status.billing, billing);
      await adapter.disconnect();
    }
  });

  it('starts provider sign-in and announces completion', async () => {
    const adapter = new CodexAdapter({ launch: launcher('fake-codex.js', 'signin') });
    const info = await adapter.login();
    assert.equal(info.url, 'https://auth.example/login');
    assert.deepEqual(await adapter.waitForLogin(), { success: true, error: null });
    await adapter.disconnect();
  });

  it('streams a turn with tool calls, declines native commands and routes tool permission', async () => {
    const argsFile = join(OPTIONS.workspace, 'codex-args.json');
    const adapter = new CodexAdapter({ launch: launcher('fake-codex.js', 'chatgpt', argsFile) });
    const { events, emit, text } = collector();
    const session = await adapter.createSession(OPTIONS, emit, policy);
    assert.deepEqual(await session.sendPrompt('draw a box'), { stopReason: 'completed' });
    assert.match(text(), /command=\{"decision":"decline"\}/);
    assert.match(text(), /elicitation=\{"action":"accept"/);
    assert.ok(events.some((e) => e.type === 'toolStarted' && e.tool === 'get_sdk_contract'));
    assert.ok(events.some((e) => e.type === 'toolFinished' && e.ok));
    const launched = JSON.parse(readFileSync(argsFile, 'utf8'));
    for (const flag of LOCKDOWN) assert.ok(launched.argv.includes(flag), flag);
    const servers = launched.argv.filter((arg: string) => arg.startsWith('mcp_servers'));
    assert.equal(servers.length, 1);
    assert.match(servers[0], /^mcp_servers=\{ framelab = \{ command = .*THORVISION_WORKSPACE = /);
    await adapter.disconnect();
  });

  it('subscription mode removes API keys from the child environment only', async () => {
    process.env.OPENAI_API_KEY = 'sk-should-not-pass';
    const argsFile = join(OPTIONS.workspace, 'codex-env.json');
    const adapter = new CodexAdapter({ launch: launcher('fake-codex.js', 'chatgpt', argsFile) });
    await adapter.accountStatus();
    assert.equal(JSON.parse(readFileSync(argsFile, 'utf8')).env.OPENAI_API_KEY, undefined);
    assert.equal(process.env.OPENAI_API_KEY, 'sk-should-not-pass');
    delete process.env.OPENAI_API_KEY;
    await adapter.disconnect();
  });

  it('maps quota exhaustion, interruption and process exit', async () => {
    let adapter = new CodexAdapter({ launch: launcher('fake-codex.js', 'quota') });
    let run = collector();
    let session = await adapter.createSession(OPTIONS, run.emit, policy);
    assert.equal((await session.sendPrompt('x')).stopReason, 'failed');
    assert.ok(run.events.some((e) => e.type === 'failed' && e.state === 'quota_exhausted'));
    await adapter.disconnect();

    adapter = new CodexAdapter({ launch: launcher('fake-codex.js', 'interrupt') });
    run = collector();
    session = await adapter.createSession(OPTIONS, run.emit, policy);
    const pending = session.sendPrompt('x');
    await new Promise((resolve) => setTimeout(resolve, 300));
    await session.cancel();
    assert.equal((await pending).stopReason, 'interrupted');
    await adapter.disconnect();

    adapter = new CodexAdapter({ launch: launcher('fake-codex.js', 'crash') });
    run = collector();
    session = await adapter.createSession(OPTIONS, run.emit, policy);
    assert.equal((await session.sendPrompt('x')).stopReason, 'failed');
    assert.ok(run.events.some((e) => e.type === 'failed' && /exited \(3\)/.test(e.message)));
  });
});

describe('Gemini adapter (ACP)', () => {
  it('reports sign-in required without prompting the model', async () => {
    const adapter = new GeminiAdapter({ launch: launcher('fake-gemini.js', 'signin') });
    assert.equal((await adapter.accountStatus()).state, 'signin_required');
    await adapter.disconnect();
  });

  it('streams a prompt, allows FrameLab tools and rejects native ones', async (t) => {
    const argsFile = join(OPTIONS.workspace, 'gemini-args.json');
    process.env.GEMINI_API_KEY = 'should-not-pass';
    const adapter = new GeminiAdapter({ launch: launcher('fake-gemini.js', 'ok', argsFile) });
    t.after(() => adapter.disconnect());
    const { events, emit, text } = collector();
    const session = await adapter.createSession(OPTIONS, emit, policy);
    assert.deepEqual(await session.sendPrompt('draw a box'), { stopReason: 'end_turn' });
    delete process.env.GEMINI_API_KEY;
    assert.match(text(), /prompt-starts-with-instructions=true/);
    assert.match(text(), /write=\{"outcome":"selected","optionId":"yes"\}/);
    assert.match(text(), /shell=\{"outcome":"selected","optionId":"no"\}/);
    assert.match(text(), /"fs":\{"readTextFile":false,"writeTextFile":false\},"terminal":false/);
    assert.match(text(), /"name":"THORVISION_WORKSPACE"/);
    assert.ok(events.some((e) => e.type === 'toolFinished' && e.ok));
    const launched = JSON.parse(readFileSync(argsFile, 'utf8'));
    assert.equal(launched.env.GEMINI_API_KEY, undefined);
    const settings = JSON.parse(readFileSync(launched.env.GEMINI_CLI_SYSTEM_SETTINGS_PATH, 'utf8'));
    assert.deepEqual(settings.tools.exclude, EXCLUDED_TOOLS);
    assert.equal(settings.security.auth.enforcedType, 'oauth-personal');
    assert.deepEqual(launched.argv, ['--acp', '--allowed-mcp-server-names', 'framelab']);
    await adapter.disconnect();
  });

  it('maps quota exhaustion, cancellation and process exit', async () => {
    let adapter = new GeminiAdapter({ launch: launcher('fake-gemini.js', 'quota') });
    let run = collector();
    let session = await adapter.createSession(OPTIONS, run.emit, policy);
    assert.equal((await session.sendPrompt('x')).stopReason, 'failed');
    assert.ok(run.events.some((e) => e.type === 'failed' && e.state === 'quota_exhausted'));
    await adapter.disconnect();

    adapter = new GeminiAdapter({ launch: launcher('fake-gemini.js', 'interrupt') });
    run = collector();
    session = await adapter.createSession(OPTIONS, run.emit, policy);
    const pending = session.sendPrompt('x');
    await new Promise((resolve) => setTimeout(resolve, 400));
    await session.cancel();
    assert.equal((await pending).stopReason, 'cancelled');
    await adapter.disconnect();

    adapter = new GeminiAdapter({ launch: launcher('fake-gemini.js', 'crash') });
    run = collector();
    session = await adapter.createSession(OPTIONS, run.emit, policy);
    assert.equal((await session.sendPrompt('x')).stopReason, 'failed');
    assert.ok(run.events.some((e) => e.type === 'failed'));
    await adapter.disconnect();
  });
});

/** Scripted stand-in for the Agent SDK's Query: one scripted reply per prompt. */
function fakeQuery(script: (prompt: string, options: Options) => Promise<SDKMessage[]> | SDKMessage[], account: object = {}) {
  const seen: { options?: Options } = {};
  const query: QueryFunction = ({ prompt, options }) => {
    seen.options = options;
    let interrupted: (() => void) | null = null;
    async function* run(): AsyncGenerator<SDKMessage, void> {
      for await (const message of prompt as AsyncIterable<SDKUserMessage>) {
        const text = message.message.content as string;
        if (text === 'wait') {
          await new Promise<void>((resolve) => (interrupted = resolve));
          yield { type: 'result', subtype: 'error_during_execution', is_error: true, stop_reason: null } as unknown as SDKMessage;
          continue;
        }
        for (const reply of await script(text, options!)) yield reply;
      }
    }
    const generator = run() as unknown as Query;
    Object.assign(generator, {
      accountInfo: async () => account,
      interrupt: async () => interrupted?.(),
    });
    return generator;
  };
  return { query, seen };
}

const assistant = (content: unknown[], error?: string) => ({ type: 'assistant', message: { content }, parent_tool_use_id: null, ...(error ? { error } : {}) }) as unknown as SDKMessage;
const result = (subtype = 'success') => ({ type: 'result', subtype, is_error: subtype !== 'success', stop_reason: subtype === 'success' ? 'end_turn' : null }) as unknown as SDKMessage;

describe('Claude adapter (Agent SDK)', () => {
  it('maps account information to connection states and billing', () => {
    assert.deepEqual([claudeStatus({ subscriptionType: 'max', email: 'u@example.com', tokenSource: 'claude.ai' }).state, claudeStatus({ subscriptionType: 'max' }).billing], ['connected', 'Claude max']);
    assert.equal(claudeStatus({ subscriptionType: 'Claude Pro' }).billing, 'Claude Pro');
    assert.equal(claudeStatus({ apiKeySource: 'ANTHROPIC_API_KEY' }).state, 'unsupported');
    assert.equal(claudeStatus({ apiProvider: 'bedrock' }).state, 'unsupported');
    assert.equal(claudeStatus({}).state, 'signin_required');
  });

  it('runs with no built-in tools, only ThorVision MCP, and no API keys', async () => {
    process.env.ANTHROPIC_API_KEY = 'sk-ant-should-not-pass';
    const decisions: unknown[] = [];
    const { query, seen } = fakeQuery(async (_prompt, options) => {
      decisions.push(await options.canUseTool!('mcp__framelab__write_project', { path: 'processor.py' }, { signal: new AbortController().signal } as any));
      decisions.push(await options.canUseTool!('Bash', { command: 'cat ~/.ssh/id_rsa' }, { signal: new AbortController().signal } as any));
      return [
        assistant([{ type: 'text', text: 'Writing the callback. ' }, { type: 'tool_use', id: 't1', name: 'mcp__framelab__write_project', input: {} }]),
        { type: 'user', message: { role: 'user', content: [{ type: 'tool_result', tool_use_id: 't1', is_error: false }] }, parent_tool_use_id: null } as unknown as SDKMessage,
        assistant([{ type: 'text', text: 'Done.' }]),
        result(),
      ];
    }, { subscriptionType: 'pro' });
    const adapter = new ClaudeAdapter(query);
    assert.equal((await adapter.accountStatus()).billing, 'Claude pro');
    const { events, emit, text } = collector();
    const session = await adapter.createSession(OPTIONS, emit, policy);
    assert.deepEqual(await session.sendPrompt('draw a box'), { stopReason: 'end_turn' });
    delete process.env.ANTHROPIC_API_KEY;
    assert.equal(text(), 'Writing the callback. Done.');
    assert.deepEqual((decisions[0] as any).behavior, 'allow');
    assert.deepEqual((decisions[1] as any).behavior, 'deny');
    assert.ok(events.some((e) => e.type === 'toolFinished' && e.tool === 'mcp__framelab__write_project' && e.ok));
    assert.deepEqual(seen.options!.tools, []);
    assert.deepEqual(seen.options!.settingSources, []);
    assert.equal(seen.options!.env!.ANTHROPIC_API_KEY, undefined);
    assert.deepEqual(Object.keys(seen.options!.mcpServers!), ['framelab']);
    await adapter.disconnect();
  });

  it('maps quota and sign-in errors, interruption and process exit', async () => {
    let { query } = fakeQuery(() => [assistant([{ type: 'text', text: '' }], 'rate_limit'), result('error_during_execution')]);
    let adapter = new ClaudeAdapter(query);
    let run = collector();
    let session = await adapter.createSession(OPTIONS, run.emit, policy);
    assert.equal((await session.sendPrompt('x')).stopReason, 'error_during_execution');
    assert.ok(run.events.some((e) => e.type === 'failed' && e.state === 'quota_exhausted'));
    await adapter.disconnect();

    ({ query } = fakeQuery(() => [assistant([], 'authentication_failed'), result('error_during_execution')]));
    adapter = new ClaudeAdapter(query);
    run = collector();
    session = await adapter.createSession(OPTIONS, run.emit, policy);
    await session.sendPrompt('x');
    assert.ok(run.events.some((e) => e.type === 'failed' && e.state === 'signin_required'));
    await adapter.disconnect();

    ({ query } = fakeQuery(() => []));
    adapter = new ClaudeAdapter(query);
    run = collector();
    session = await adapter.createSession(OPTIONS, run.emit, policy);
    const pending = session.sendPrompt('wait');
    await new Promise((resolve) => setTimeout(resolve, 50));
    await session.cancel();
    assert.equal((await pending).stopReason, 'error_during_execution');
    await adapter.disconnect();

    ({ query } = fakeQuery(() => {
      throw new Error('Claude Code process exited with code 1');
    }));
    adapter = new ClaudeAdapter(query);
    run = collector();
    session = await adapter.createSession(OPTIONS, run.emit, policy);
    assert.equal((await session.sendPrompt('x')).stopReason, 'failed');
    assert.ok(run.events.some((e) => e.type === 'failed' && /exited with code 1/.test(e.message)));
  });
});
