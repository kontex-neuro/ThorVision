import { expect, mock, test, type TestBody } from 'claude-code/testing';

const digest = 'a'.repeat(64);
const IMPLICIT_DEMOS = { schema: 'framelab.demo-catalog/1', options: [
  { id: 'person-detector', label: 'Person detector',
    digest: '3f9efcf3af48e9163c79383be646a0ff08c7dc1f472b66ace16e46825241b671',
    prompt: 'Detect people in the camera view. Draw a box around each person and show the current count.' },
  { id: 'three-zone-entries', label: 'Three-zone entries',
    digest: '899455ad03397c276760f984a5721ae25c4d1ce9c1a1a600faf1ea4376f36374',
    prompt: 'Count people entering the left, centre and right thirds of the camera view. Show the three zones and their entry totals.' },
] };
const PANE = { plugin: 'thorvision-framelab', component: 'Pane', requestId: 'framelab',
  viewport: { columns: 100, rows: 40 }, props: { title: 'ThorVision FrameLab', isFocused: true,
    bodyColumns: 80, placement: 'inline', scroll: { offset: 0, bodyRows: 30 }, view: {} } } as const;

const COMMAND = { command: 'framelab', args: '', origin: { kind: 'composer' },
  presentation: { isFullscreen: false, columns: 100 } } as const;
function stub(on: Parameters<TestBody>[1], ready = true, { status = {}, beginError = '', requests = [], legacyReview = false,
  demoCatalog, statusError = '', mcpTools = [], promptText = '', panelConnection }: {
  status?: object; beginError?: string; requests?: { path: string; body: unknown }[]; legacyReview?: boolean;
  demoCatalog?: object; statusError?: string; mcpTools?: string[]; promptText?: string;
  panelConnection?: () => { api: string; url: string; token: string };
} = {}) {
  const calls: string[] = [];
  on('mcp.call', ($, e) => {
    expect(e.server).toBe('plugin:thorvision-framelab:framelab');
    mcpTools.push(e.tool);
    if (e.tool === 'get_demo_recipes') return { value: demoCatalog
      ? { isError: false, content: [{ type: 'text', text: JSON.stringify(demoCatalog) }] }
      : { isError: true, content: [{ type: 'text', text: 'Unknown tool: get_demo_recipes' }] } };
    return { value: { isError: false, content: [{ type: 'text', text: JSON.stringify(panelConnection?.() || { api: 'http://127.0.0.1:1234', url: 'http://localhost:1234/#test', token: 'test' }) }] } };
  });
  on('http.fetch', ($, e) => {
    const path = e.url.split('/api/')[1]!; calls.push(path);
    const body = typeof e.init?.body === 'string' ? JSON.parse(e.init.body) : undefined;
    requests.push({ path, body });
    if (path === 'status' && statusError)
      return { value: { ok: false, status: 503, headers: {}, text: JSON.stringify({ error: statusError }) } };
    if (path === 'disconnected/review' && legacyReview)
      return { value: { ok: false, status: 404, headers: {}, text: JSON.stringify({ error: 'Unknown operation' }) } };
    if (path === 'authoring/begin' && beginError)
      return { value: { ok: false, status: 409, headers: {}, text: JSON.stringify({ error: beginError }) } };
    const value = path === 'status' ? { camera: 'cam', cameras: [{ id: 'cam', name: 'Camera', discovered: true }],
      packages: [{ digest, ready }], state: { cameras: { cam: {} } }, candidate: { name: 'Demo', digest },
      hasProject: true, busy: false, authoring: false, operation: { message: 'Ready' }, ...status }
      : path === 'disconnected/review' ? { id: 'disconnected-review', action: 'stop', camera: body.camera, activeDigest: digest, revision: 7 }
      : path === 'review' ? { id: 'review-1', action: body?.action || 'apply', camera: legacyReview ? '/dev/v4l/by-path/old' : 'cam', digest: body?.digest || digest, activeDigest: legacyReview ? digest : null }
      : path === 'frame.json' ? { frame: { jpeg: 'AAAA', receivedAt: Date.now(), sequence: '1', width: 640, height: 480 } } : { ok: true };
    return { value: { ok: true, status: 200, headers: {}, text: JSON.stringify(value) } };
  });
  on('ui.open', () => ({ value: { isPlaced: true } }));
  on('ui.close', () => ({ value: undefined }));
  on('prompt.submit', ($, e) => ({ text: e.text, context: e.context }));
  on('turn.complete', () => ({ text: '' }));
  on('prompt.read', () => ({ value: { text: promptText, cursor: promptText.length } }));
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

test('the native camera picker excludes disconnected identities and keeps detected device paths', async ($, on) => {
  const camera = '/dev/v4l/by-path/current', second = '/dev/v4l/by-path/second', old = '/dev/v4l/by-path/old';
  const requests: { path: string; body: unknown }[] = [];
  const calls = stub(on, true, { requests, status: { camera, cameras: [
    { id: camera, name: 'Current camera', discovered: true },
    { id: second, name: 'Second camera', discovered: true },
    { id: old, name: old, discovered: false, state: { previous: { digest } } },
  ], state: { cameras: {} } } });
  await $.command.run(COMMAND);
  const ui = await $.ui.mount({ ...PANE, surface: 'desktop' });
  const picker = await ui.find({ key: 'camera' });
  expect(picker?.props.options).toEqual([{ value: camera, label: 'Current camera' }, { value: second, label: 'Second camera' }]);
  expect(picker).toBeDefined();
  await ui.select({ key: 'camera', value: second });
  expect(requests.filter((request) => request.path === 'camera')).toEqual([{ path: 'camera', body: { camera: second } }]);
  expect(calls).not.toContain('execute');
  await ui.unmount();
});

test('one detected camera does not offer a history entry as another camera', async ($, on) => {
  const old = '/dev/v4l/by-path/old';
  const calls = stub(on, true, { status: { cameras: [
    { id: 'cam', name: 'Camera', discovered: true },
    { id: old, name: old, discovered: false, state: { previous: { digest } } },
  ] } });
  await $.command.run(COMMAND);
  const ui = await $.ui.mount({ ...PANE, surface: 'desktop' });
  expect(await ui.find({ key: 'camera' })).toBeUndefined();
  expect(await ui.find({ type: 'Text', text: 'Analyses on disconnected cameras' })).toBeUndefined();
  expect(calls).not.toContain('execute');
  await ui.unmount();
});

test('a disconnected active analysis has its own exact Stop review and requires confirmation', async ($, on) => {
  const old = '/dev/v4l/by-path/old';
  const requests: { path: string; body: unknown }[] = [];
  const calls = stub(on, true, { requests, status: { cameras: [
    { id: 'cam', name: 'Camera', discovered: true },
    { id: old, name: old, discovered: false, state: { active: { digest, name: 'Old tracker' } } },
  ], state: { cameras: { cam: {}, [old]: { active: { digest, name: 'Old tracker' } } } } } });
  await $.command.run(COMMAND);
  const ui = await $.ui.mount({ ...PANE, surface: 'desktop' });
  expect(await ui.find({ key: 'camera' })).toBeUndefined();
  expect(await ui.find({ type: 'Text', text: 'Analyses on disconnected cameras' })).toBeDefined();
  await ui.press({ key: 'stop-disconnected-' + old });
  expect(requests.filter((request) => request.path === 'disconnected/review')).toEqual([{ path: 'disconnected/review', body: { camera: old } }]);
  expect(calls).not.toContain('camera');
  expect(calls).not.toContain('execute');
  expect(await ui.find({ type: 'Text', text: 'Stop the analysis on the disconnected camera?' })).toBeDefined();
  await ui.press({ key: 'confirm' });
  expect(requests.filter((request) => request.path === 'execute')).toEqual([{ path: 'execute', body: { id: 'disconnected-review' } }]);
  await ui.unmount();
});

test('an empty project can choose an earlier tested analysis and review its exact revision before running', async ($, on) => {
  const older = 'b'.repeat(64), untested = 'c'.repeat(64);
  const requests: { path: string; body: unknown }[] = [];
  const calls = stub(on, true, { requests, status: { hasProject: false, candidate: null, packages: [
    { digest, ready: true, manifest: { name: 'Saved tracker' }, report: { frames: 12, time: Date.now() / 1000 } },
    { digest: older, ready: true, manifest: { name: 'Saved tracker' }, report: { frames: 24, time: Date.now() / 1000 } },
    { digest: untested, ready: false, manifest: { name: 'Not tested' } },
  ] } });
  await $.command.run(COMMAND);
  const ui = await $.ui.mount({ ...PANE, surface: 'desktop' });
  expect(await ui.find({ type: 'Text', text: 'Run an earlier analysis' })).toBeDefined();
  expect((await ui.find({ key: 'package' }))?.props.options).toEqual([
    { value: '', label: 'Choose an analysis' },
    { value: digest, label: 'Saved tracker · tested just now · aaaaaaaa' },
    { value: older, label: 'Saved tracker · tested just now · bbbbbbbb' },
  ]);
  expect(await ui.find({ key: 'run-other' })).toBeUndefined();
  await ui.select({ key: 'package', value: older });
  expect(await ui.find({ type: 'Text', text: 'Passed a test run on 24 sample frames.' })).toBeDefined();
  expect(calls).not.toContain('review');
  expect(calls).not.toContain('execute');
  await ui.press({ key: 'run-other' });
  expect(requests.filter((request) => request.path === 'review')).toEqual([{ path: 'review', body: { action: 'apply', digest: older } }]);
  expect(calls).not.toContain('execute');
  expect(await ui.find({ type: 'Text', text: 'Run the new analysis on Camera?' })).toBeDefined();
  await ui.press({ key: 'details' });
  expect(await ui.find({ type: 'Text', text: older })).toBeDefined();
  await ui.press({ key: 'confirm' });
  expect(requests.filter((request) => request.path === 'execute')).toEqual([{ path: 'execute', body: { id: 'review-1' } }]);
  await ui.unmount();
});

test('earlier-analysis selection survives polling and clears when readiness or availability changes', async ($, on) => {
  const clock = mock.clock(on);
  const saved = { digest, ready: true, manifest: { name: 'Saved tracker' }, report: { frames: 12 } };
  const status = { hasProject: false, candidate: null, packages: [saved] };
  const calls = stub(on, true, { status });
  on('command.register', () => ({ value: undefined }));
  on('session.start', ($, event) => ({ cwd: event.cwd }));
  await $.session.start({ cwd: 'C:/FrameLab', surface: 'terminal', isInteractive: true });
  await $.command.run(COMMAND);
  const ui = await $.ui.mount({ ...PANE, surface: 'terminal' });
  await ui.select({ key: 'package', value: digest });
  await clock.advance(2000);
  expect((await ui.find({ key: 'package' }))?.props.value).toBe(digest);
  expect(await ui.find({ key: 'run-other' })).toBeDefined();
  saved.ready = false;
  await clock.advance(2000);
  expect(await ui.find({ key: 'package' })).toBeUndefined();
  expect(await ui.find({ key: 'run-other' })).toBeUndefined();
  saved.ready = true;
  await clock.advance(2000);
  expect((await ui.find({ key: 'package' }))?.props.value).toBe('');
  await ui.select({ key: 'package', value: digest });
  status.packages = [];
  await clock.advance(2000);
  status.packages = [saved];
  await clock.advance(2000);
  expect((await ui.find({ key: 'package' }))?.props.value).toBe('');
  expect(await ui.find({ key: 'run-other' })).toBeUndefined();
  expect(calls).not.toContain('execute');
  await ui.unmount();
});

test('an empty earlier-analysis catalog explains why no selection is offered', async ($, on) => {
  stub(on, false, { status: { hasProject: false, candidate: null, packages: [] } });
  await $.command.run(COMMAND);
  const ui = await $.ui.mount({ ...PANE, surface: 'desktop' });
  expect(await ui.find({ type: 'Text', text: 'No tested analyses are available yet.' })).toBeDefined();
  expect(await ui.find({ key: 'package' })).toBeUndefined();
  expect(await ui.find({ key: 'run-other' })).toBeUndefined();
  await ui.unmount();
});

for (const blockedBy of ['busy', 'authoring', 'no camera', 'disconnected analysis'] as const) {
  test(`an earlier analysis cannot be reviewed while blocked by ${blockedBy}`, async ($, on) => {
    const old = '/dev/v4l/by-path/old';
    const calls = stub(on, true, { status: { hasProject: false, candidate: null,
      busy: blockedBy === 'busy', authoring: blockedBy === 'authoring',
      camera: blockedBy === 'no camera' ? undefined : 'cam',
      cameras: blockedBy === 'no camera' ? [] : [
        { id: 'cam', name: 'Camera', discovered: true },
        { id: old, name: old, discovered: false, state: { active: { digest } } },
      ], operation: blockedBy === 'disconnected analysis'
        ? { state: 'failed', message: `analytics is already active on ${old}` } : { state: 'idle', message: '' },
    } });
    await $.command.run(COMMAND);
    const ui = await $.ui.mount({ ...PANE, surface: 'desktop' });
    if (blockedBy === 'busy' || blockedBy === 'authoring') expect(await ui.find({ key: 'package' })).toBeUndefined();
    else await ui.select({ key: 'package', value: digest });
    expect(await ui.find({ key: 'run-other' })).toBeUndefined();
    expect(calls).not.toContain('review');
    expect(calls).not.toContain('execute');
    if (blockedBy === 'disconnected analysis') expect(await ui.find({ type: 'Text', text: 'Stop the disconnected analysis first' })).toBeDefined();
    await ui.unmount();
  });
}

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

test('a new project shows raw camera video before an analysis is prepared', async ($, on) => {
  const calls = stub(on, false, { status: { hasProject: false, candidate: {},
    state: { cameras: { cam: {} } }, preview: { kind: 'raw', receivedAt: Date.now() } } });
  await $.command.run(COMMAND);
  const desktop = await $.ui.mount({ ...PANE, surface: 'desktop' });
  expect(await desktop.find({ type: 'Svg' })).toBeDefined();
  expect(await desktop.find({ type: 'Text', text: 'Camera live' })).toBeDefined();
  expect(await desktop.find({ type: 'Text', text: 'Live camera preview · no analysis is running on this image.' })).toBeDefined();
  expect(calls.includes('execute')).toBe(false);
  expect(calls.includes('prepare')).toBe(false);
  await desktop.unmount();
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
  const raw = '$.mcp.call(plugin:thorvision-framelab:framelab, open_control_panel) refused: The server-side auto mode classifier gave no verdict';
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

for (const running of [false, true]) {
  test(`the demo mod offers exactly two recipe starters in a ${running ? 'running' : 'new empty'} project`, async ($, on) => {
    const options = [
      { id: 'person-detector', label: 'Person detector', prompt: 'Restore exact bundled person-detector.' },
      { id: 'three-zone-entries', label: 'Three-zone entries', prompt: 'Restore exact bundled three-zone-entries.' },
    ];
    const calls = stub(on, true, { status: { hasProject: false, candidate: null, demo: { options },
      state: { cameras: { cam: running ? { active: { digest }, run: { state: 'running' } } : {} } } } });
    await $.command.run(COMMAND);
    for (const surface of ['terminal', 'desktop'] as const) {
      const ui = await $.ui.mount({ ...PANE, surface });
      expect(await ui.find({ type: 'Text', text: 'Choose a demo' })).toBeDefined();
      const starters = (await ui.findAll({ type: 'Button' })).filter((item) => item.key?.startsWith('starter-'));
      expect(starters.map((item) => item.key)).toEqual(options.map((item) => 'starter-' + item.label));
      for (const option of options) await ui.press({ key: 'starter-' + option.label });
      expect(calls).toContain('fill:' + options[0].prompt);
      expect(calls).toContain('fill:' + options[1].prompt);
      expect(calls).not.toContain('authoring/begin');
      expect(calls).not.toContain('execute');
      expect(calls).not.toContain('prepare');
      await ui.unmount();
    }
  });
}

test('an offline camera still exposes both locally bundled demo prompts without pretending video is healthy', async ($, on) => {
  const options = [
    { id: 'person-detector', label: 'Person detector', digest, prompt: 'Restore exact bundled person-detector; config {} and human review only.' },
    { id: 'three-zone-entries', label: 'Three-zone entries', digest: 'b'.repeat(64), prompt: 'Restore exact bundled three-zone-entries; config {} and human review only.' },
  ];
  const mcpTools: string[] = [];
  const calls = stub(on, true, { mcpTools, statusError: 'device unavailable: connection refused',
    demoCatalog: { schema: 'framelab.demo-catalog/1', options } });
  await $.command.run(COMMAND);
  expect(mcpTools).toEqual(['open_control_panel', 'get_demo_recipes']);
  for (const surface of ['desktop', 'terminal'] as const) {
    const ui = await $.ui.mount({ ...PANE, surface });
    expect(await ui.find({ type: 'Text', text: 'Not connected' })).toBeDefined();
    expect(await ui.find({ key: 'refresh' })).toBeDefined();
    expect(await ui.find({ type: 'Text', text: 'Choose a demo' })).toBeDefined();
    const starters = (await ui.findAll({ type: 'Button' })).filter((item) => item.key?.startsWith('starter-'));
    expect(starters.map((item) => item.key)).toEqual(options.map((item) => 'starter-' + item.label));
    expect(await ui.find({ type: 'Svg' })).toBeUndefined();
    expect(await ui.find({ type: 'Text', text: 'Camera live' })).toBeUndefined();
    for (const option of options) await ui.press({ key: 'starter-' + option.label });
    for (const option of options) expect(calls).toContain('fill:' + option.prompt);
    expect(calls).not.toContain('authoring/begin');
    expect(calls).not.toContain('prepare');
    expect(calls).not.toContain('execute');
    await ui.unmount();
  }
});

for (const demoCatalog of [undefined, { api: 'old-server-response' }, { schema: 'framelab.demo-catalog/1', options: [{ id: 'person-detector' }] }]) {
  test(`an offline standard runtime ignores ${demoCatalog ? 'invalid' : 'unavailable'} optional demo metadata`, async ($, on) => {
    const calls = stub(on, true, { demoCatalog, statusError: 'device unavailable: connection refused' });
    await $.command.run(COMMAND);
    const ui = await $.ui.mount({ ...PANE, surface: 'desktop' });
    expect(await ui.find({ type: 'Text', text: 'Not connected' })).toBeDefined();
    expect(await ui.find({ key: 'refresh' })).toBeDefined();
    expect(await ui.find({ type: 'Text', text: 'Choose a demo' })).toBeUndefined();
    expect((await ui.findAll({ type: 'Button' })).filter((item) => item.key?.startsWith('starter-')).length).toBe(0);
    expect(calls).not.toContain('execute');
    await ui.unmount();
  });
}

for (const demo of IMPLICIT_DEMOS.options) {
  test(`the ${demo.id} starter keeps composer text plain and binds its exact recipe only in hidden context`, async ($, on) => {
    const mcpTools: string[] = [];
    const calls = stub(on, true, { demoCatalog: IMPLICIT_DEMOS, mcpTools,
      status: { hasProject: false, candidate: null, demo: IMPLICIT_DEMOS } });
    await $.command.run(COMMAND);
    const ui = await $.ui.mount({ ...PANE, surface: 'desktop' });
    await ui.press({ key: 'starter-' + demo.label });
    expect(calls).toContain('fill:' + demo.prompt);
    expect(/package|digest|restore_demo_recipe|get_demo_recipes|[a-f0-9]{64}/i.test(demo.prompt)).toBe(false);
    expect(calls).not.toContain('authoring/begin');
    expect(mcpTools).not.toContain('restore_demo_recipe');
    const result = await $.prompt.submit({ text: demo.prompt, context: ['Earlier hidden context.'],
      wait: false, origin: { kind: 'composer' } });
    expect(result.text).toBe(demo.prompt);
    const hidden = result.context?.join('\n') || '';
    expect(hidden).toContain('Earlier hidden context.');
    expect(hidden).toContain('restore_demo_recipe with id "' + demo.id + '"');
    expect(hidden).toContain(demo.digest);
    expect(hidden).toContain('configuration is exactly {}');
    expect(hidden).toContain('Do not generate, rewrite, or edit implementation code.');
    expect(hidden).toContain('Report environment drift');
    expect(hidden).toContain('never activate it yourself');
    expect(calls).toContain('authoring/begin');
    expect(calls).not.toContain('execute');
    await ui.unmount();
  });
}

test('a demo appended to other composer text is recognised as a complete paragraph without rewriting the message', async ($, on) => {
  const demo = IMPLICIT_DEMOS.options[1], existing = 'Please keep the current analysis running until I review the result.';
  const calls = stub(on, true, { demoCatalog: IMPLICIT_DEMOS, promptText: existing,
    status: { hasProject: false, candidate: null, demo: IMPLICIT_DEMOS } });
  await $.command.run(COMMAND);
  const ui = await $.ui.mount({ ...PANE, surface: 'desktop' });
  await ui.press({ key: 'starter-' + demo.label });
  expect(calls).toContain('fill:\n' + demo.prompt);
  const text = existing + '\n' + demo.prompt;
  const result = await $.prompt.submit({ text, wait: false, origin: { kind: 'composer' } });
  expect(result.text).toBe(text);
  expect(result.context?.join('\n')).toContain(demo.digest);
  await ui.unmount();
});

test('mentioning a demo request inside another sentence does not silently select its recipe', async ($, on) => {
  stub(on, true, { demoCatalog: IMPLICIT_DEMOS });
  await $.command.run(COMMAND);
  const text = 'Explain the wording "' + IMPLICIT_DEMOS.options[0].prompt + '" without changing anything.';
  const result = await $.prompt.submit({ text, wait: false, origin: { kind: 'composer' } });
  expect(result.text).toBe(text);
  expect(result.context?.join('\n').includes('restore_demo_recipe')).toBe(false);
});

test('both demo paragraphs require clarification without picking or starting an editing operation', async ($, on) => {
  const calls = stub(on, true, { demoCatalog: IMPLICIT_DEMOS });
  await $.command.run(COMMAND);
  const text = IMPLICIT_DEMOS.options.map((item) => item.prompt).join('\n\n');
  const result = await $.prompt.submit({ text, wait: false, origin: { kind: 'composer' } });
  expect(result.text).toBe(text);
  expect(result.context?.join('\n')).toContain('Ask which demo the user wants');
  expect(result.context?.join('\n').includes('expected package digest')).toBe(false);
  expect(calls).not.toContain('authoring/begin');
  expect(calls).not.toContain('prepare');
  expect(calls).not.toContain('execute');
});

test('a refused editing operation keeps the plain demo message and does not attach instructions to restore it', async ($, on) => {
  stub(on, true, { demoCatalog: IMPLICIT_DEMOS, beginError: 'A device operation is in progress.' });
  await $.command.run(COMMAND);
  const text = IMPLICIT_DEMOS.options[0].prompt;
  const result = await $.prompt.submit({ text, wait: false, origin: { kind: 'composer' } });
  expect(result.text).toBe(text);
  expect(result.context?.join('\n')).toContain('A device operation is in progress.');
  expect(result.context?.join('\n').includes('restore_demo_recipe')).toBe(false);
});

function expectNoCameraAuthoringOrHookActuation(calls: string[], mcpTools: string[]) {
  for (const path of ['authoring/begin', 'authoring/finish', 'prepare', 'execute']) expect(calls).not.toContain(path);
  expect(mcpTools).not.toContain('get_digital_io');
  expect(mcpTools).not.toContain('set_digital_outputs');
}

test('a direct OUT2 hold request bypasses camera authoring and adds operator guidance without actuating hardware', async ($, on) => {
  const mcpTools: string[] = [];
  const calls = stub(on, true, { mcpTools, beginError: 'Camera editing is unavailable.', statusError: 'camera API unavailable' });
  await $.command.run(COMMAND);
  const text = 'Turn on the LED connected to digital OUT2 until I say stop.';
  const result = await $.prompt.submit({ text, wait: false, origin: { kind: 'composer' } });
  expect(result.text).toBe(text);
  const hidden = result.context?.join('\n') || '';
  expect(hidden).toContain('direct operator-requested digital I/O');
  expect(hidden).toContain('{"mask":2,"values":2}');
  expect(hidden).toContain('preserve all other outputs');
  expect(hidden).toContain('not camera-status RGB LED2/GPIO20');
  expect(hidden).toContain('authoritative compatibility');
  expect(hidden).toContain('uncertain HIGH must never be replayed automatically');
  expect(hidden).toContain('operator\'s visual LED observation');
  expect(hidden.includes('This is a ThorVision FrameLab authoring session')).toBe(false);
  await $.turn.complete({ turnId: 'dio-high', answer: 'Completion is uncertain.', durationMs: 1, isAborted: false, reason: 'answer' });
  expectNoCameraAuthoringOrHookActuation(calls, mcpTools);
});

test('Stop clears only the last explicit output target while status reads never retarget it', async ($, on) => {
  const mcpTools: string[] = [];
  const calls = stub(on, true, { mcpTools });
  await $.command.run(COMMAND);
  await $.prompt.submit({ text: 'Turn on digital OUT2 until I say stop.', wait: false, origin: { kind: 'composer' } });
  const read = await $.prompt.submit({ text: 'Read digital OUT1 status.', wait: false, origin: { kind: 'composer' } });
  expect(read.context?.join('\n')).toContain('This request is read-only');
  const result = await $.prompt.submit({ text: 'Stop', wait: false, origin: { kind: 'composer' } });
  expect(result.text).toBe('Stop');
  const hidden = result.context?.join('\n') || '';
  expect(hidden).toContain('{"mask":2,"values":0}');
  expect(hidden).toContain('only if the conversation still confirms that target');
  expect(hidden).toContain('new action even after uncertain HIGH');
  expect(hidden).toContain('already confirms the requested LOW');
  expect(hidden).toContain('never stop the camera analysis');
  expect(hidden.includes('{"mask":1,')).toBe(false);
  await $.turn.complete({ turnId: 'dio-stop', answer: 'LOW confirmation is required.', durationMs: 1, isAborted: false, reason: 'answer' });
  expectNoCameraAuthoringOrHookActuation(calls, mcpTools);
});

test('an explicit OUT2 LOW needs no earlier HIGH and a read alone never authorizes a later inferred target', async ($, on) => {
  const mcpTools: string[] = [];
  const calls = stub(on, true, { mcpTools });
  await $.command.run(COMMAND);
  await $.prompt.submit({ text: 'Read digital OUT2 status.', wait: false, origin: { kind: 'composer' } });
  const ambiguous = await $.prompt.submit({ text: 'Turn it off', wait: false, origin: { kind: 'composer' } });
  expect(ambiguous.context?.join('\n')).toContain('No prior explicit digital-output control target is known');
  expect(ambiguous.context?.join('\n').includes('{"mask":2')).toBe(false);
  const low = await $.prompt.submit({ text: 'Turn off the LED connected to digital OUT2.', wait: false, origin: { kind: 'composer' } });
  expect(low.context?.join('\n')).toContain('{"mask":2,"values":0}');
  expectNoCameraAuthoringOrHookActuation(calls, mcpTools);
});

test('Stop analysis stays on the camera workflow even after an OUT2 operator request', async ($, on) => {
  const calls = stub(on);
  await $.command.run(COMMAND);
  await $.prompt.submit({ text: 'Turn on digital OUT2.', wait: false, origin: { kind: 'composer' } });
  const result = await $.prompt.submit({ text: 'Stop the analysis.', wait: false, origin: { kind: 'composer' } });
  expect(result.text).toBe('Stop the analysis.');
  expect(result.context?.join('\n')).toContain('This is a ThorVision FrameLab authoring session');
  expect(result.context?.join('\n').includes('requested output mask')).toBe(false);
  expect(calls).toContain('authoring/begin');
  expect(calls).not.toContain('execute');
});

test('a later camera authoring turn makes bare Stop ambiguous rather than clearing an older output target', async ($, on) => {
  const calls = stub(on);
  await $.command.run(COMMAND);
  await $.prompt.submit({ text: 'Turn on digital OUT2.', wait: false, origin: { kind: 'composer' } });
  await $.prompt.submit({ text: 'Track the animal in the camera view.', wait: false, origin: { kind: 'composer' } });
  const result = await $.prompt.submit({ text: 'Stop', wait: false, origin: { kind: 'composer' } });
  expect(result.context?.join('\n')).toContain('No prior explicit digital-output control target is known');
  expect(result.context?.join('\n').includes('{"mask":')).toBe(false);
  expect(calls).not.toContain('execute');
});

test('a replacement selected MCP connection cannot inherit the old bare Stop target', async ($, on) => {
  let token = 'device-session-one';
  const mcpTools: string[] = [];
  const calls = stub(on, true, { mcpTools, panelConnection: () => ({ api: 'http://127.0.0.1:1234',
    url: 'http://localhost:1234/#' + token, token }) });
  await $.command.run(COMMAND);
  await $.prompt.submit({ text: 'Turn on digital OUT2.', wait: false, origin: { kind: 'composer' } });
  token = 'replacement-device-session';
  const result = await $.prompt.submit({ text: 'Stop', wait: false, origin: { kind: 'composer' } });
  expect(result.context?.join('\n')).toContain('No prior explicit digital-output control target is known');
  expect(result.context?.join('\n').includes('{"mask":')).toBe(false);
  expectNoCameraAuthoringOrHookActuation(calls, mcpTools);
});

test('clearing a DIO-only chat forgets its output target without finalizing a camera project or clearing a latch', async ($, on) => {
  const mcpTools: string[] = [];
  const calls = stub(on, true, { mcpTools });
  on('session.end', ($, event) => ({ sessionId: event.sessionId }));
  await $.command.run(COMMAND);
  await $.prompt.submit({ text: 'Turn on digital OUT2.', wait: false, origin: { kind: 'composer' } });
  await $.session.end({ reason: 'clear', sessionId: 'old-chat', resume: { id: 'old-chat' } });
  await $.command.run(COMMAND);
  const result = await $.prompt.submit({ text: 'Turn it off', wait: false, origin: { kind: 'composer' } });
  expect(result.context?.join('\n')).toContain('No prior explicit digital-output control target is known');
  expect(result.context?.join('\n').includes('{"mask":')).toBe(false);
  expectNoCameraAuthoringOrHookActuation(calls, mcpTools);
});

for (const text of [
  'Turn on camera-status RGB LED2.', 'Set GPIO20 HIGH.', 'Turn on LED2.',
  'Turn on OUT20.', 'Turn on digital OUT5.', 'Turn on OUT1-4.',
]) {
  test(`an ambiguous or unqualified hardware name does not become an OUT2 write: ${text}`, async ($, on) => {
    const mcpTools: string[] = [];
    const calls = stub(on, true, { mcpTools });
    await $.command.run(COMMAND);
    const result = await $.prompt.submit({ text, wait: false, origin: { kind: 'composer' } });
    expect(result.text).toBe(text);
    expect(result.context?.join('\n').includes('{"mask":')).toBe(false);
    expect(result.context?.join('\n').includes('digital I/O on the selected')).toBe(false);
    expectNoCameraAuthoringOrHookActuation(calls, mcpTools);
  });
}

for (const text of [
  'When a person enters the centre, turn digital OUT2 HIGH.',
  'Write a callback that sets digital OUT2 HIGH.',
  'Track people and turn digital OUT2 on.',
]) {
  test(`mixed or conditional output automation requires clarification: ${text}`, async ($, on) => {
    const mcpTools: string[] = [];
    const calls = stub(on, true, { mcpTools });
    await $.command.run(COMMAND);
    const result = await $.prompt.submit({ text, wait: false, origin: { kind: 'composer' } });
    expect(result.text).toBe(text);
    expect(result.context?.join('\n')).toContain('Clarify the independent operator action first');
    expect(result.context?.join('\n')).toContain('Camera callbacks stay isolated');
    expect(result.context?.join('\n').includes('{"mask":')).toBe(false);
    expectNoCameraAuthoringOrHookActuation(calls, mcpTools);
  });
}
