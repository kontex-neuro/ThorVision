import { expect, test, type TestBody } from 'claude-code/testing';

const digest = 'a'.repeat(64);
const PANE = { plugin: 'thorvision-control', component: 'Pane', requestId: 'thorvision',
  viewport: { columns: 100, rows: 40 }, props: { title: 'ThorVision', isFocused: true,
    bodyColumns: 80, placement: 'inline', scroll: { offset: 0, bodyRows: 30 }, view: {} } } as const;

const COMMAND = { command: 'thorvision', args: '', origin: { kind: 'composer' },
  presentation: { isFullscreen: false, columns: 100 } } as const;
function stub(on: Parameters<TestBody>[1], ready = true, { status = {}, beginError = '' }: { status?: object; beginError?: string } = {}) {
  const calls: string[] = [];
  on('mcp.call', ($, e) => {
    expect(e.server).toBe('plugin:thorvision-control:thorvision');
    return { value: { isError: false, content: [{ type: 'text', text: JSON.stringify({ api: 'http://127.0.0.1:1234', url: 'http://localhost:1234/#test', token: 'test' }) }] } };
  });
  on('http.fetch', ($, e) => {
    const path = e.url.split('/api/')[1]!; calls.push(path);
    if (path === 'authoring/begin' && beginError)
      return { value: { ok: false, status: 409, headers: {}, text: JSON.stringify({ error: beginError }) } };
    const value = path === 'status' ? { camera: 'cam', cameras: [{ id: 'cam', name: 'Camera' }],
      packages: [{ digest, ready }], state: { cameras: { cam: {} } }, candidate: { name: 'Demo', digest },
      hasProject: true, busy: false, authoring: false, operation: { message: 'Ready' }, ...status }
      : path === 'review' ? { id: 'review-1', action: 'apply', camera: 'cam', digest, activeDigest: null }
      : path === 'frame.json' ? { frame: { jpeg: 'AAAA', receivedAt: Date.now(), sequence: '1', width: 640, height: 480 } } : { ok: true };
    return { value: { ok: true, status: 200, headers: {}, text: JSON.stringify(value) } };
  });
  on('ui.open', () => ({ value: { isPlaced: true } }));
  on('ui.close', () => ({ value: undefined }));
  on('prompt.submit', ($, e) => ({ text: e.text, context: e.context }));
  on('turn.complete', () => ({ text: '' }));
  on('prompt.read', () => ({ value: { text: '', cursor: 0 } }));
  on('prompt.fill', ($, e) => { calls.push('fill:' + e.text); return { isFilled: true }; });
  return calls;
}

test('native buttons require a separate exact revision confirmation on terminal and desktop', async ($, on) => {
  const calls = stub(on);
  await $.command.run(COMMAND);
  for (const surface of ['terminal', 'desktop'] as const) {
    const ui = await $.ui.mount({ ...PANE, surface });
    expect(await ui.find({ key: 'run' })).toBeDefined();
    await ui.press({ key: 'run' });
    expect(calls.filter((path) => path === 'execute').length).toBe(surface === 'terminal' ? 0 : 1);
    expect(await ui.find({ type: 'Text', text: 'Run the new analysis on Camera?' })).toBeDefined();
    expect(await ui.find({ key: 'run' })).toBeUndefined();
    expect(await ui.find({ type: 'Text', text: digest })).toBeUndefined();
    await ui.press({ key: 'details' });
    expect(await ui.find({ type: 'Text', text: digest })).toBeDefined();
    await ui.press({ key: 'confirm' });
    await ui.unmount();
  }
  expect(calls.filter((path) => path === 'execute').length).toBe(2);
});

test('exact identities stay behind technical details, including at narrow width', async ($, on) => {
  stub(on);
  await $.command.run(COMMAND);
  const ui = await $.ui.mount({ ...PANE, surface: 'desktop', props: { ...PANE.props, bodyColumns: 30 } });
  expect(await ui.find({ type: 'Text', text: 'New analysis ready' })).toBeDefined();
  expect(await ui.find({ type: 'Text', text: digest })).toBeUndefined();
  await ui.press({ key: 'details' });
  expect(await ui.find({ type: 'Text', text: digest })).toBeDefined();
  await ui.unmount();
});

test('untested candidate never has a Run button', async ($, on) => {
  stub(on, false);
  await $.command.run(COMMAND);
  const ui = await $.ui.mount({ ...PANE, surface: 'terminal' });
  expect(await ui.find({ key: 'run' })).toBeUndefined();
  expect(await ui.find({ key: 'prepare' })).toBeDefined();
  await ui.unmount();
});

test('ordinary prompts inject the workflow and complete the authoring lifecycle', async ($, on) => {
  const calls = stub(on);
  await $.command.run(COMMAND);
  const result = await $.prompt.submit({ text: 'Track people', wait: false, origin: { kind: 'composer' } });
  expect(result.text).toBe('Track people');
  expect(result.context?.join('')).toContain('prepare_extension');
  await $.turn.complete({ turnId: 't', answer: 'Created', durationMs: 10, isAborted: false, reason: 'answer' });
  expect(calls).toContain('authoring/begin');
  expect(calls).toContain('authoring/finish');
  expect(calls).not.toContain('execute');
});

test('installing the plugin does not intercept unrelated conversations', async ($, on) => {
  const calls = stub(on);
  const result = await $.prompt.submit({ text: 'Review my code', wait: false, origin: { kind: 'composer' } });
  expect(result.text).toBe('Review my code');
  expect(result.context).toBeUndefined();
  expect(calls).toEqual([]);
});

test('leaving analytics mode stops prompt injection without stopping the camera', async ($, on) => {
  const calls = stub(on);
  await $.command.run(COMMAND);
  await $.command.run({ ...COMMAND, command: 'thorvision-off' });
  calls.length = 0;
  const result = await $.prompt.submit({ text: 'Review my code', wait: false, origin: { kind: 'composer' } });
  expect(result.context).toBeUndefined();
  expect(calls).toEqual([]);
});

test('an unavailable editing turn still reaches Claude with the reason instead of being dropped', async ($, on) => {
  const calls = stub(on, true, { status: { busy: true }, beginError: 'A device operation is in progress.' });
  await $.command.run(COMMAND);
  const result = await $.prompt.submit({ text: 'Can we continue?', wait: false, origin: { kind: 'composer' } });
  expect(result.text).toBe('Can we continue?');
  expect(result.context?.join('')).toContain('A device operation is in progress.');
  expect(result.context?.join('')).not.toContain('Finish changes with prepare_extension');
  await $.turn.complete({ turnId: 't', answer: 'Waiting', durationMs: 10, isAborted: false, reason: 'answer' });
  expect(calls).not.toContain('authoring/finish');
});

test('leaving analytics mode releases an editing window a lost turn left open', async ($, on) => {
  const calls = stub(on, true, { status: { authoring: true } });
  await $.command.run(COMMAND);
  await $.command.run({ ...COMMAND, command: 'thorvision-off' });
  expect(calls).toContain('authoring/finish');
  calls.length = 0;
  const result = await $.prompt.submit({ text: 'Review my code', wait: false, origin: { kind: 'composer' } });
  expect(result.context).toBeUndefined();
});

test('task starters put an editable request into Claude’s own message box', async ($, on) => {
  const calls = stub(on, true, { status: { hasProject: false, candidate: null } });
  await $.command.run(COMMAND);
  const ui = await $.ui.mount({ ...PANE, surface: 'desktop' });
  expect(await ui.find({ type: 'Text', text: 'Tell Claude what to analyse' })).toBeDefined();
  await ui.press({ key: 'starter-Track the animal' });
  expect(calls.some((call) => call.startsWith('fill:Track the mouse'))).toBe(true);
  expect(calls).not.toContain('authoring/begin');
  await ui.unmount();
});

test('a running analysis offers additions to it instead of new experiments', async ($, on) => {
  const running = { active: { digest, name: 'Tracker' }, run: { state: 'running', processed_fps: 5, target_fps: 5 } };
  const calls = stub(on, true, { status: { state: { cameras: { cam: running } }, preview: { receivedAt: Date.now() } } });
  await $.command.run(COMMAND);
  const ui = await $.ui.mount({ ...PANE, surface: 'desktop' });
  expect(await ui.find({ type: 'Text', text: 'What would you like to change?' })).toBeDefined();
  expect(await ui.find({ key: 'starter-Open field test' })).toBeUndefined();
  await ui.press({ key: 'starter-Add freezing' });
  expect(calls.some((call) => call.startsWith('fill:Add to the analysis running on the camera'))).toBe(true);
  await ui.unmount();
});

test('the desktop panel shows the live preview; the terminal links to it', async ($, on) => {
  stub(on);
  await $.command.run(COMMAND);
  const desktop = await $.ui.mount({ ...PANE, surface: 'desktop' });
  expect(await desktop.find({ type: 'Svg' })).toBeDefined();
  await desktop.unmount();
  const terminal = await $.ui.mount({ ...PANE, surface: 'terminal' });
  expect(await terminal.find({ type: 'Link' })).toBeDefined();
  await terminal.unmount();
});

test('slow or faulted processing is named, not left to inference', async ($, on) => {
  const running = { active: { digest, name: 'Tracker' }, run: { state: 'running', below_target: true, processed_fps: 3, target_fps: 5 } };
  stub(on, true, { status: { state: { cameras: { cam: running } }, preview: { receivedAt: Date.now() } } });
  await $.command.run(COMMAND);
  let ui = await $.ui.mount({ ...PANE, surface: 'desktop' });
  expect(await ui.find({ type: 'Text', text: 'Slower than target' })).toBeDefined();
  expect(await ui.find({ type: 'Text', text: 'below its target of 5' })).toBeDefined();
  await ui.unmount();
});

test('a permission refusal reads as one plain sentence, with the raw error behind details', async ($, on) => {
  const raw = '$.mcp.call(plugin:thorvision-control:thorvision, open_control_panel) refused: The server-side auto mode classifier gave no verdict';
  on('mcp.call', () => ({ value: { isError: true, content: [{ type: 'text', text: raw }] } }));
  on('ui.open', () => ({ value: { isPlaced: true } }));
  await $.command.run(COMMAND);
  const ui = await $.ui.mount({ ...PANE, surface: 'desktop' });
  expect(await ui.find({ type: 'Text', text: 'Blocked by a permission check' })).toBeDefined();
  expect(await ui.find({ type: 'Text', text: 'classifier' })).toBeUndefined();
  await ui.press({ key: 'details' });
  expect(await ui.find({ type: 'Text', text: 'classifier' })).toBeDefined();
  await ui.unmount();
});
