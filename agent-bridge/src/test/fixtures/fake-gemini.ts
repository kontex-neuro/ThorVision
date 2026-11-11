// ACP fixture standing in for `gemini --acp`; behaviour is chosen by FAKE_SCENARIO.
import { writeFileSync } from 'node:fs';
import { Readable, Writable } from 'node:stream';
import { RequestError, agent, methods, ndJsonStream } from '@agentclientprotocol/sdk';

const scenario = process.env.FAKE_SCENARIO ?? 'ok';
if (process.env.FAKE_ARGS_FILE) writeFileSync(process.env.FAKE_ARGS_FILE, JSON.stringify({ argv: process.argv.slice(2), env: process.env }));
let cancelled: (() => void) | null = null;
let lastSession: unknown = null;

const app = agent({ name: 'fake-gemini' })
  .onRequest(methods.agent.initialize, ({ params }) => {
    lastSession = { clientCapabilities: params.clientCapabilities };
    return { protocolVersion: 1, authMethods: [{ id: 'oauth-personal', name: 'Log in with Google' }, { id: 'gemini-api-key', name: 'Use Gemini API key' }] };
  })
  .onRequest(methods.agent.authenticate, () => ({}))
  .onRequest(methods.agent.session.new, ({ params }) => {
    if (scenario === 'signin') throw RequestError.authRequired();
    lastSession = { ...(lastSession as object), mcpServers: params.mcpServers };
    return { sessionId: 'session-1' };
  })
  .onNotification(methods.agent.session.cancel, () => cancelled?.())
  .onRequest(methods.agent.session.prompt, async ({ params, client }) => {
    const update = (value: any) => client.notify(methods.client.session.update, { sessionId: params.sessionId, update: value });
    if (scenario === 'quota') throw RequestError.internalError(undefined, 'Quota exceeded: RESOURCE_EXHAUSTED');
    if (scenario === 'crash') process.exit(4);
    const prompt = (params.prompt[0] as any).text as string;
    await update({ sessionUpdate: 'agent_message_chunk', content: { type: 'text', text: `prompt-starts-with-instructions=${prompt.startsWith('You build ThorVision FrameLab')} ` } });
    await update({ sessionUpdate: 'agent_message_chunk', content: { type: 'text', text: `session=${JSON.stringify(lastSession)} ` } });
    await update({ sessionUpdate: 'tool_call', toolCallId: 'call-1', title: 'write_project (thorvision MCP Server)', status: 'pending', rawInput: { path: 'processor.py' } });
    const allowed = await client.request(methods.client.session.requestPermission, {
      sessionId: params.sessionId,
      toolCall: { toolCallId: 'call-1', title: 'write_project (thorvision MCP Server)', rawInput: { path: 'processor.py' } },
      options: [{ optionId: 'yes', name: 'Allow', kind: 'allow_once' }, { optionId: 'no', name: 'Reject', kind: 'reject_once' }],
    });
    const shell = await client.request(methods.client.session.requestPermission, {
      sessionId: params.sessionId,
      toolCall: { toolCallId: 'call-2', title: 'run_shell_command: cat ~/.ssh/id_rsa' },
      options: [{ optionId: 'yes', name: 'Allow', kind: 'allow_once' }, { optionId: 'no', name: 'Reject', kind: 'reject_once' }],
    });
    await update({ sessionUpdate: 'tool_call_update', toolCallId: 'call-1', title: 'write_project (thorvision MCP Server)', status: 'completed' });
    await update({ sessionUpdate: 'agent_message_chunk', content: { type: 'text', text: `write=${JSON.stringify(allowed.outcome)} shell=${JSON.stringify(shell.outcome)}` } });
    if (scenario === 'interrupt') {
      await new Promise<void>((resolve) => (cancelled = resolve));
      return { stopReason: 'cancelled' };
    }
    return { stopReason: 'end_turn' };
  });

app.connect(ndJsonStream(Writable.toWeb(process.stdout) as WritableStream<Uint8Array>, Readable.toWeb(process.stdin) as ReadableStream<Uint8Array>));
