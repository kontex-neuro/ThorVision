// Protocol fixture standing in for `codex app-server`; behaviour is chosen by FAKE_SCENARIO.
import { writeFileSync } from 'node:fs';
import { JsonRpcPeer } from '../../jsonrpc.js';

const scenario = process.env.FAKE_SCENARIO ?? 'chatgpt';
if (process.env.FAKE_ARGS_FILE) writeFileSync(process.env.FAKE_ARGS_FILE, JSON.stringify({ argv: process.argv.slice(2), env: process.env }));
const peer = new JsonRpcPeer(process.stdin, process.stdout, false);
let initialized = false;
let interrupted: (() => void) | null = null;
const thread = 'thread-1';

peer.onNotification((method) => {
  if (method === 'initialized') initialized = true;
});

peer.onRequest(async (method, params) => {
  if (method !== 'initialize' && !initialized) throw new Error('not initialized');
  switch (method) {
    case 'initialize':
      return { userAgent: 'fake-codex/0.165.0', codexHome: '/tmp', platformFamily: 'windows', platformOs: 'windows' };
    case 'account/read':
      if (scenario === 'signin') return { account: null, requiresOpenaiAuth: true };
      if (scenario === 'apikey') return { account: { type: 'apiKey' }, requiresOpenaiAuth: true };
      return { account: { type: 'chatgpt', email: 'user@example.com', planType: 'plus' }, requiresOpenaiAuth: true };
    case 'account/login/start':
      setTimeout(() => peer.notify('account/login/completed', { loginId: 'login-1', success: true, error: null, onboardingEntrypoint: null }), 50);
      return { type: 'chatgpt', loginId: 'login-1', authUrl: 'https://auth.example/login' };
    case 'thread/start':
      return { thread: { id: thread }, model: 'gpt', modelProvider: 'openai', serviceTier: null, disabledPluginIds: [], cwd: params.cwd,
        instructionSources: [], approvalPolicy: params.approvalPolicy, approvalsReviewer: 'user', sandbox: { type: 'readOnly' }, reasoningEffort: null };
    case 'turn/interrupt':
      interrupted?.();
      return {};
    case 'turn/start':
      setTimeout(() => void turn(), 10);
      return { turn: { id: 'turn-1', status: 'inProgress' } };
    default:
      throw new Error(`unexpected ${method}`);
  }
});

async function turn(): Promise<void> {
  const base = { threadId: thread, turnId: 'turn-1' };
  const completed = (status: string, error: unknown = null) =>
    peer.notify('turn/completed', { threadId: thread, turn: { id: 'turn-1', status, error } });
  if (scenario === 'quota') {
    const error = { message: 'Usage limit reached', codexErrorInfo: 'usageLimitExceeded', additionalDetails: null, misalignment: null };
    peer.notify('error', { ...base, error, willRetry: false });
    return completed('failed', error);
  }
  if (scenario === 'crash') process.exit(3);
  peer.notify('item/started', { ...base, startedAtMs: 0, item: { type: 'mcpToolCall', id: 'call-1', server: 'thorvision', tool: 'get_sdk_contract', status: 'inProgress', arguments: {} } });
  const command = await peer.request('item/commandExecution/requestApproval', { ...base, itemId: 'cmd-1', command: 'cat ~/.ssh/id_rsa' });
  const elicitation = await peer.request('mcpServer/elicitation/request', { ...base, serverName: 'thorvision', message: 'Allow test_candidate?' });
  peer.notify('item/completed', { ...base, item: { type: 'mcpToolCall', id: 'call-1', server: 'thorvision', tool: 'get_sdk_contract', status: 'completed', arguments: {} } });
  peer.notify('item/agentMessage/delta', { ...base, itemId: 'msg-1', delta: `command=${JSON.stringify(command)} ` });
  peer.notify('item/agentMessage/delta', { ...base, itemId: 'msg-1', delta: `elicitation=${JSON.stringify(elicitation)}` });
  if (scenario === 'interrupt') {
    await new Promise<void>((resolve) => (interrupted = resolve));
    return completed('interrupted');
  }
  completed('completed');
}
