import assert from 'node:assert/strict';
import { PassThrough } from 'node:stream';
import { describe, it } from 'node:test';
import { JsonRpcPeer, RpcError } from '../jsonrpc.js';
import { Bridge, PROTOCOL_VERSION, thorvisionTool } from '../main.js';
import type { AgentAdapter, AgentEvent, AgentId, PermissionAsker } from '../types.js';

function pair() {
  const toBridge = new PassThrough();
  const toClient = new PassThrough();
  return { bridge: new JsonRpcPeer(toBridge, toClient), client: new JsonRpcPeer(toClient, toBridge) };
}

class FakeAdapter implements AgentAdapter {
  decisions: boolean[] = [];
  constructor(readonly id: AgentId) {}
  readonly name = 'Fake';
  async discover() { return { id: this.id, name: this.name, available: true, version: '1', runtime: 'fake', problems: [] }; }
  async connect() { return this.accountStatus(); }
  async accountStatus() { return { agent: this.id, state: 'connected' as const, billing: 'Test plan', account: null, detail: '' }; }
  async login() { return { instructions: 'none' }; }
  async disconnect() {}
  async createSession(_options: unknown, emit: (event: AgentEvent) => void, ask: PermissionAsker) {
    return {
      id: 'inner',
      sendPrompt: async (text: string) => {
        this.decisions.push(await ask({ tool: 'mcp__thorvision__test_candidate', title: 'Use test_candidate' }));
        this.decisions.push(await ask({ tool: 'Bash', title: 'Run cat ~/.ssh/id_rsa' }));
        emit({ type: 'message', text: `echo ${text}` });
        emit({ type: 'completed', stopReason: 'end_turn' });
        return { stopReason: 'end_turn' };
      },
      cancel: async () => {},
      close: async () => {},
    };
  }
}

describe('JSON-RPC peer', () => {
  it('correlates requests, carries errors and delivers notifications', async () => {
    const { bridge, client } = pair();
    bridge.onRequest(async (method, params) => {
      if (method === 'fail') throw new RpcError(-32010, 'nope');
      return { echo: params };
    });
    const notes: unknown[] = [];
    client.onNotification((method, params) => notes.push([method, params]));
    assert.deepEqual(await client.request('echo', { a: 1 }), { echo: { a: 1 } });
    await assert.rejects(client.request('fail'), (error: RpcError) => error.code === -32010);
    bridge.notify('hello', { n: 2 });
    await new Promise((resolve) => setImmediate(resolve));
    assert.deepEqual(notes, [['hello', { n: 2 }]]);
    bridge.close();
    client.close();
  });
});

describe('Bridge', () => {
  it('matches ThorVision tools however the provider names them', () => {
    assert.equal(thorvisionTool({ tool: 'mcp__thorvision__write_project', title: '' }), 'write_project');
    assert.equal(thorvisionTool({ tool: 'thorvision', title: 'write_project (thorvision MCP Server)' }), 'write_project');
    assert.equal(thorvisionTool({ tool: 'Bash', title: 'Run write_project_backup.sh' }), null);
    assert.equal(thorvisionTool({ tool: 'run_shell_command', title: 'cat secrets' }), null);
  });

  it('serves the versioned protocol and applies the permission policy', async () => {
    const { bridge, client } = pair();
    const adapter = new FakeAdapter('codex');
    new Bridge(bridge, { codex: adapter, claude: new FakeAdapter('claude'), gemini: new FakeAdapter('gemini') });
    await assert.rejects(client.request('bridge.initialize', { protocolVersion: 99 }));
    const hello = await client.request('bridge.initialize', { protocolVersion: PROTOCOL_VERSION });
    assert.equal(hello.sdk.sdk_version, '0.1.0');
    assert.equal((await client.request('agents.discover')).length, 3);
    assert.equal((await client.request('agent.accountStatus', { agent: 'codex' })).billing, 'Test plan');
    await assert.rejects(client.request('session.create', { agent: 'codex', camera: 'c' }));

    const events: any[] = [];
    client.onNotification((method, params) => method === 'session.event' && events.push(params));
    const { sessionId } = await client.request('session.create', { agent: 'codex', camera: 'c', workspace: 'w', extensionsUrl: 'http://127.0.0.1:1' });
    assert.deepEqual(await client.request('session.prompt', { sessionId, text: 'hi' }), { stopReason: 'end_turn' });
    assert.deepEqual(adapter.decisions, [true, false]);
    assert.ok(events.every((event) => event.sessionId === sessionId && event.agent === 'codex'));
    assert.ok(events.some((event) => event.type === 'message' && event.text === 'echo hi'));

    // "ask" mode routes ThorVision tool permission to the user.
    const asked = await client.request('session.create', { agent: 'codex', camera: 'c', workspace: 'w', extensionsUrl: 'http://127.0.0.1:1', approval: 'ask' });
    client.onNotification((method, params) => {
      if (method === 'session.event' && params.type === 'permissionRequested') void client.request('permission.respond', { requestId: params.requestId, allow: false });
    });
    adapter.decisions = [];
    await client.request('session.prompt', { sessionId: asked.sessionId, text: 'again' });
    assert.deepEqual(adapter.decisions, [false, false]);
    await assert.rejects(client.request('permission.respond', { requestId: 'stale', allow: true }));
    await client.request('session.close', { sessionId });
    await assert.rejects(client.request('session.prompt', { sessionId, text: 'x' }));
    bridge.close();
    client.close();
  });
});
