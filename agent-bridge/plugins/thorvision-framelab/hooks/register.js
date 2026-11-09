// The native agent owns chat and authentication. This mod owns human controls.
let connection, snapshot, pending, timer, frameTimer, polling = false, acting = false, notice = '', connectionNotice = '';
let active = false, details = false, editing = false, frame, framePolling = false, frameUnsupported = false, videoSince;
const SERVER = 'plugin:thorvision-framelab:framelab';
const unavailable = (reason) => `ThorVision editing is unavailable for this turn: ${reason} Do not modify the analytics extension project or call prepare_extension this turn. Answer normally, and if the user asks for analytics changes, tell them this reason and to check /framelab.`;
const GUIDE = `This is a ThorVision FrameLab authoring session. Use the ThorVision MCP tools to inspect the SDK, device capabilities and existing project before editing. Write only the extension project. Preserve original frame association and camera isolation. Use real processing; never substitute simulated detections. Finish changes with prepare_extension: it validates, reuses compatible dependencies automatically, installs through the temporary PC proxy when needed, and tests the exact candidate. Report actual errors and measured performance. The mod also checks preparation when your turn finishes. The developer pre-deploys and maintains the service; all user operations use its API. Do not ask the user to connect over SSH, request reuse or run validation manually. If the API is unavailable, report its error for the developer; do not deploy the service or fall back to SSH. Activation, rollback and cleanup belong to the human ThorVision controls; never call the local control HTTP actions or activate through shell commands. Tell the user to open /framelab and click Run on camera for a tested candidate. Device tests use fixture frames, not a detection-accuracy benchmark.`;

async function connect($) {
  if (connection) return;
  const result = await $.mcp.call(SERVER, 'open_control_panel', {});
  if (result.isError) throw new Error(result.content?.[0]?.text || 'ThorVision MCP is unavailable.');
  connection = JSON.parse(result.content.find((item) => item.type === 'text').text);
}
async function api($, path, body) {
  await connect($);
  let response;
  try { response = await $.http.fetch(connection.api + '/api/' + path, {
    method: body === undefined ? 'GET' : 'POST',
    headers: { Authorization: 'Bearer ' + connection.token, ...(body === undefined ? {} : { 'Content-Type': 'application/json' }) },
    ...(body === undefined ? {} : { body: JSON.stringify(body) }),
  }); } catch (error) { connection = undefined; throw error; }
  if (response.status === 401) connection = undefined;
  const value = JSON.parse(response.text);
  if (!response.ok) throw new Error(value.error || 'Device operation failed');
  return value;
}
async function refresh($) {
  if (polling) return;
  polling = true;
  try {
    try { snapshot = await api($, 'status'); }
    catch (error) { if (connection) throw error; snapshot = await api($, 'status'); }
    connectionNotice = '';
  }
  catch (error) { snapshot = undefined; connectionNotice = error.message; }
  finally { polling = false; $.ui.invalidate('ui.render'); }
}
const refused = () => !connection && /refused|classifier|denied/i.test(connectionNotice);
// The panel preview: one processed frame about every second. An older bridge has no
// frame endpoint; the panel then keeps the larger-view link instead of retrying forever.
async function refreshFrame($) {
  if (framePolling || frameUnsupported || !connection) return;
  framePolling = true;
  try { frame = (await api($, 'frame.json')).frame ?? undefined; }
  catch (error) { if (/Unknown operation/.test(error.message)) frameUnsupported = true; frame = undefined; }
  finally { framePolling = false; $.ui.invalidate('ui.render'); }
}
async function act($, path, body, review = false) {
  if (acting) return;
  acting = true; notice = ''; $.ui.invalidate('ui.render');
  try {
    const value = await api($, path, body);
    pending = review ? value : undefined;
    details = false;
    await refresh($);
  } catch (error) { notice = error.message; }
  finally { acting = false; $.ui.invalidate('ui.render'); }
}
// Task starters go into Claude's own message box to edit and send; the panel has no prompt box.
async function suggest($, request) {
  const box = await $.prompt.read();
  const typed = box.text.trim() !== '';
  const result = await $.prompt.fill({ text: typed ? '\n' + request : request, mode: typed ? 'append' : 'replace' });
  $.ui.toast(result.isFilled ? 'Edit the request in Claude’s message box, then send it.' : 'Close the open dialog, then try the starter again.');
}

// Presentation: the scientist's words first, exact identities only under Technical details.
const short = (digest) => digest ? digest.slice(0, 8) : '';
const cameraName = (name = '') => name.includes(': ') ? name.slice(name.lastIndexOf(': ') + 2) : name;
function ago(seconds) {
  if (!seconds) return '';
  const minutes = Math.max(0, Math.round((Date.now() / 1000 - seconds) / 60));
  if (minutes < 1) return 'just now';
  if (minutes < 60) return minutes + ' min ago';
  if (minutes < 60 * 24) return Math.round(minutes / 60) + ' h ago';
  return Math.round(minutes / 60 / 24) + ' d ago';
}
function describe(s, digest, fallback) {
  const item = s.packages.find((p) => p.digest === digest);
  const name = item?.manifest?.name || fallback || 'Analysis';
  const when = ago(item?.report?.time);
  return { name, note: [when && 'tested ' + when, short(digest)].filter(Boolean).join(' · ') };
}
// Standard experiment paradigms the device can do with classical tracking and motion at about 5 FPS.
// Behaviours such as grooming or rearing need a trained model the camera does not have, so no starter promises them.
const STARTERS = [
  ['Set up the arena', 'Find the edges of the arena and draw them on the video. The arena is 40 cm wide, so work out how many pixels make one centimetre and show it. Warn me on the video if the image is too dark, too bright or out of focus for tracking.'],
  ['Track the animal', 'Track the mouse in the arena. Draw its position and its path over the last few seconds on the video, and save its position for every frame so I can analyse it later.'],
  ['Open field test', 'Run an open field test on the mouse. The arena is 40 cm wide; treat the outer 20% as the edge and the rest as the centre. Show the distance travelled in centimetres, the current speed and the time in the centre and at the edge on the video, and log these values every minute.'],
  ['Detect freezing', 'Detect freezing: the mouse makes almost no movement for at least 1 second. Show FREEZING on the video while it lasts, keep a running freezing percentage, and log when each freezing bout starts and ends.'],
  ['Y-maze alternation', 'The arena is a Y-maze with arms A, B and C. Detect each time the mouse fully enters an arm, show the sequence of arm entries on the video, and calculate the spontaneous alternation percentage. Log every arm entry with its time.'],
  ['Elevated plus maze', 'The arena is an elevated plus maze with two open and two closed arms. Count entries into the open and closed arms and the time spent in each arm and in the centre. Show them on the video and log every arm entry and exit.'],
  ['Moving or resting', 'Classify each moment as moving, resting or freezing from how much the mouse moves. Show the current state on the video and log when each state starts and ends.'],
  ['Count zone entries', 'Count how often the mouse enters a zone I describe, for example the left third of the arena. Show the count on the video and log the time of each entry.'],
];
const CHANGE = ['Change the analysis', 'Change the analysis running on the camera so that '];
// Small additions to whatever already runs; they keep its tracking, so tests stay quick.
const FOLLOW_UPS = [
  ['Add distance and speed', 'Add to the analysis running on the camera, keeping everything it already does: show the distance travelled in centimetres and the current speed on the video. The arena is 40 cm wide.'],
  ['Add a centre zone', 'Add to the analysis running on the camera, keeping everything it already does: treat the outer 20% of the arena as the edge and the rest as the centre, and show the time spent in each on the video.'],
  ['Add freezing', 'Add to the analysis running on the camera, keeping everything it already does: detect freezing, almost no movement for at least 1 second, show FREEZING while it lasts and keep a running freezing percentage.'],
];

// Is processing healthy? Problems are always named, never left for the user to infer.
function health(s, state) {
  const run = state.run, fault = state.fault || run?.fault;
  if (fault) return { word: 'Fault', color: 'error', warning: 'The analysis reported a fault: ' + (fault.message || JSON.stringify(fault)) };
  if (!state.active) return { word: 'Idle', color: 'subtle' };
  if (run?.state === 'starting') return { word: 'Starting…', color: 'warning' };
  if (run?.state !== 'running') return { word: 'Not running', color: 'error', warning: 'The analysis is installed but not processing video' + (run?.state ? ` (status: ${run.state}).` : '.') };
  // A bridge that has just started has no frame yet; that is not a stall until it lasts.
  if (s.preview) videoSince = undefined; else videoSince ??= Date.now();
  if (!s.preview && Date.now() - videoSince < 10000) return { word: 'Connecting video…', color: 'subtle' };
  if (!s.preview || Date.now() - s.preview.receivedAt > 5000) return { word: 'Video paused', color: 'warning', warning: 'No new video for a few seconds. The camera or the analysis may have stalled.' };
  if (run.below_target) return { word: 'Slower than target', color: 'warning',
    warning: `Processing ${run.processed_fps?.toFixed(1) ?? '—'} frames per second, below its target of ${run.target_fps}.` };
  return { word: 'Live', color: 'success' };
}
// The next step: one card, chosen from backend facts. Readiness stays the backend's decision.
function guide(s, candidate, state) {
  const op = s.operation?.state;
  if (s.canCancel || op === 'preparing' || op === 'cancelling') return { title: 'Testing on the camera…', message: s.operation.message, cancel: true };
  if (op === 'running-operation') return { title: 'Switching the camera…', message: s.operation.message };
  if (s.authoring) return { title: 'Claude is working on your request…', message: 'When Claude finishes a change, it is tested on the camera automatically.' };
  const digest = s.candidate?.digest;
  const fresh = s.hasProject && digest && (s.candidate.dirty || (digest !== state.active?.digest && digest !== state.previous?.digest));
  if (op === 'failed' && (fresh || !candidate?.ready)) return { title: 'That didn’t work', color: 'error',
    message: s.operation.message + ' The camera keeps running what it ran before.',
    action: candidate?.ready && !s.candidate?.dirty ? 'run' : 'prepare',
    starters: [['Ask Claude to fix it', `The last test on the camera failed with this error: ${s.operation.message} Please fix the analysis and test it again.`]] };
  if (fresh && candidate?.ready && !s.candidate.dirty) return { title: 'New analysis ready', color: 'suggestion',
    message: `${describe(s, digest, s.candidate.name).name} passed a test run on ${candidate.report?.frames ?? 'the'} sample frames. Run it, then check the result in the live preview.`, action: 'run' };
  if (fresh) return { title: 'New analysis not tested yet', message: op === 'cancelled' ? s.operation.message : 'Test it on the camera before running it.', action: 'prepare' };
  if (state.active) return { title: 'What would you like to change?', message: 'Add to what runs now, or describe a change or a different analysis in Claude.', starters: [CHANGE, ...FOLLOW_UPS] };
  return { title: 'Tell Claude what to analyse', message: 'Pick a starting point. It goes into Claude’s message box for you to edit and send.', starters: STARTERS };
}

async function openPanel($) {
    active = true;
    await refresh($);
    await refreshFrame($);
    await $.ui.open({ id: 'framelab', title: 'ThorVision FrameLab', focus: true, closeOnEscape: true, rows: 30, columns: 52 });
    return {};
}

async function closePanel($) {
    if (connection) await refresh($);
    if (snapshot?.busy) return { text: 'Wait for the current device operation to finish first.' };
    // An editing window left open by a lost turn must not trap the user in analytics mode.
    if (snapshot?.authoring) { try { await api($, 'authoring/finish', { aborted: true }); } catch {} }
    active = false;
    editing = false;
    pending = undefined;
    frame = undefined;
    await $.ui.close({ id: 'framelab' });
    return { text: 'FrameLab authoring mode is off. The camera keeps running. Use /framelab to return.' };
}

export function register(on) {
  on('session.start', async ($, e, next) => {
    timer?.cancel();
    frameTimer?.cancel();
    // Without a connection this also retries, so the panel recovers once the MCP server starts.
    // A permission refusal is not retried by itself: each retry would ask the person's permission check again.
    timer = $.clock.every(2000, async () => { if (active && !refused()) await refresh($); });
    frameTimer = $.clock.every(1000, async () => { if (active) await refreshFrame($); });
    for (const name of ['framelab', 'thorvision'])
      await $.command.register({ name, description: 'ThorVision FrameLab: research workflows and live video', immediate: true });
    for (const name of ['framelab-off', 'thorvision-off'])
      await $.command.register({ name, description: 'Leave FrameLab authoring mode for this conversation', immediate: true });
    return next(e);
  });
  on('session.end', async ($, e, next) => {
    if (active && connection) { try { await api($, 'authoring/finish', { aborted: true }); } catch {} }
    pending = undefined;
    active = false;
    if (!['clear', 'resume'].includes(e.reason)) { timer?.cancel(); frameTimer?.cancel(); }
    return next(e);
  });

  on('command.run', { command: 'framelab' }, openPanel);
  on('command.run', { command: 'thorvision' }, openPanel);

  on('command.run', { command: 'framelab-off' }, closePanel);
  on('command.run', { command: 'thorvision-off' }, closePanel);
  on('prompt.submit', async ($, e, next) => {
    if (!active) return next(e);
    editing = false;
    // Never drop the prompt: a dropped prompt looks like Claude stopped answering.
    try { await api($, 'authoring/begin', {}); editing = true; }
    catch (error) { return next({ ...e, context: [...(e.context || []), unavailable(error.message)] }); }
    return next({ ...e, context: [...(e.context || []), GUIDE] });
  }).catch(($, e, next) => next(active
    ? { ...e, context: [...(e.context || []), unavailable('the editing turn could not start.')] } : e));
  on('turn.complete', async ($, e, next) => {
    if (active && editing && !e.agentId && connection) {
      editing = false;
      try { await api($, 'authoring/finish', { aborted: e.isAborted === true }); await refresh($); }
      catch (error) { notice = error.message; $.ui.invalidate('ui.render'); }
    }
    return next(e);
  });
  on('ui.render', { component: 'Pane' }, async ($, e, next) => {
    if (e.requestId !== 'framelab') return next(e);
    const { Box, Text, Button, Select, Link, Svg } = $.ui.resolve(e);
    const text = (children, props = {}) => Text({ ...props, children: [].concat(children) });
    const column = (children, gap = 0) => Box({ flexDirection: 'column', gap, children: children.filter(Boolean) });
    const row = (children) => Box({ flexDirection: 'row', flexWrap: 'wrap', columnGap: 'sm', rowGap: 'xs', children: children.filter(Boolean) });
    const card = (children, color = 'subtle') => Box({ flexDirection: 'column', gap: 1, borderStyle: 'round', borderColor: color, paddingX: 1, children: children.filter(Boolean) });
    const toggle = () => Button({ key: 'details', label: details ? 'Hide technical details' : 'Technical details',
      plain: true, dimColor: true, onPress: () => { details = !details; $.ui.invalidate('ui.render'); } });
    const technical = (lines) => details && column(lines.map(([label, value]) =>
      column([text(label, { dimColor: true }), text(String(value ?? '—'), { mono: true, wrap: 'wrap' })])));
    const larger = (label) => connection && Link({ href: connection.url, label });

    // Offline: one plain sentence about why and what to do; the raw error only under Technical details.
    const starting = !connectionNotice || /no connected MCP tool/.test(connectionNotice);
    const offline = starting ? ['Connecting…', 'warning', 'Starting the ThorVision service. This panel retries by itself.']
      : refused() ? ['Blocked by a permission check', 'error', 'Claude Code’s permission mode did not let this panel connect to the camera service. Press Reconnect to try again. To stop this happening, allow the ThorVision panel’s connection in your permission settings, or use another permission mode.']
      : ['Not connected', 'error', 'The ThorVision service did not answer. Check that the camera device is on and reachable, then press Reconnect.'];
    if (!snapshot) return column([
      column([text(offline[0], { weight: 'semibold', color: offline[1], size: 'heading' }),
        text(offline[2], { wrap: 'wrap' }),
        text('Your camera keeps its current analysis while this panel is offline.', { dimColor: true, wrap: 'wrap' })]),
      Button({ key: 'refresh', label: 'Reconnect', variant: 'primary', onPress: () => { notice = ''; return refresh($); } }),
      !starting && toggle(),
      !starting && technical([['Error', connectionNotice]]),
    ], 1);

    const s = snapshot, state = s.state.cameras[s.camera] || {}, busy = acting || s.busy || s.authoring;
    const cam = s.cameras.find((item) => item.id === s.camera);
    const where = cam ? cameraName(cam.name) : 'this camera';
    const candidate = s.packages.find((item) => item.digest === s.candidate?.digest);
    const current = state.active && describe(s, state.active.digest, state.active.name);
    const status = health(s, state);
    const alert = notice && text('That didn’t go through: ' + notice, { color: 'error', wrap: 'wrap' });

    // 1. Camera and its health in one word.
    const header = column([
      Box({ flexDirection: 'row', flexWrap: 'wrap', justifyContent: 'space-between', columnGap: 'md', children: [
        text(cam ? cameraName(cam.name) : 'No camera selected', { weight: 'semibold', size: 'heading' }),
        text(acting ? 'Working…' : status.word, { color: acting ? 'warning' : status.color, weight: 'medium' })] }),
      s.cameras.length > 1 && !busy && !pending ? Select({ key: 'camera', label: 'Camera', value: s.camera,
        options: s.cameras.map((item) => ({ value: item.id, label: cameraName(item.name) })),
        onSelect: (camera) => { pending = undefined; frame = undefined; return act($, 'camera', { camera }); } })
        : !cam && text('Choose a camera in the larger view.', { dimColor: true }),
    ]);

    // Confirmation: one focused card in plain sentences, until the user answers.
    if (pending && !busy) {
      const target = pending.action === 'stop' ? undefined : describe(s, pending.digest, s.candidate?.name);
      const now = pending.activeDigest ? describe(s, pending.activeDigest, state.active?.name) : undefined;
      const tested = s.packages.find((p) => p.digest === pending.digest)?.report;
      // Same-named analyses are told apart by when they were tested.
      const label = (item) => item.name + (target && now && target.name === now.name ? ` (${item.note})` : '');
      const copy = {
        apply: [`Run the new analysis on ${where}?`, 'Run on camera', [
          now ? `It replaces ${label(now)}. You can go back to it afterwards.` : 'Nothing is running on this camera now.',
          tested?.passed && `It passed a test run on ${tested.frames} sample frames. Check the result in the live preview.`]],
        rollback: [`Go back to the previous analysis on ${where}?`, 'Go back', [
          `${label(target)} replaces ${now ? label(now) : 'what is running now'}. You can switch again afterwards.`]],
        stop: [`Stop the analysis on ${where}?`, 'Stop analysis', [
          `${now?.name || 'The running analysis'} stops. It stays on the device, so you can run it again later.`]],
      }[pending.action];
      return column([
        header,
        card([
          text(copy[0], { weight: 'semibold', size: 'heading', wrap: 'wrap' }),
          text(copy[2].filter(Boolean).join(' '), { wrap: 'wrap' }),
          row([Button({ key: 'confirm', label: copy[1], variant: 'primary', onPress: () => act($, 'execute', { id: pending.id }) }),
            Button({ key: 'dismiss', label: 'Cancel', variant: 'secondary', onPress: () => { pending = undefined; details = false; $.ui.invalidate('ui.render'); } })]),
        ], 'suggestion'),
        alert,
        toggle(),
        technical([['Camera path', pending.camera], ['Exact revision', pending.digest],
          ['Replaces', pending.activeDigest || 'none'], ['Camera revision', pending.revision]]),
      ], 1);
    }

    // 2. The result: the live processed preview with one line about what produces it.
    const svg = Svg && frame && `<svg xmlns="http://www.w3.org/2000/svg" width="${frame.width || 640}" height="${frame.height || 480}" viewBox="0 0 ${frame.width || 640} ${frame.height || 480}"><image href="data:image/jpeg;base64,${frame.jpeg}" width="100%" height="100%"/></svg>`;
    const preview = !Svg ? undefined
      : svg && svg.length <= 131072 ? Svg({ source: svg, alt: `Live processed video from ${where}` })
      : card([text(frameUnsupported ? 'Restart Claude to see the live video here.' : state.active ? 'Waiting for video…' : 'The live video appears here once an analysis runs.', { dimColor: true, wrap: 'wrap' })]);
    const run = state.run?.state === 'running' ? state.run : undefined;
    const caption = current
      ? text([current.name, run?.processed_fps != null && run.processed_fps.toFixed(1) + ' FPS',
        run?.latency_ms_p95 != null && Math.round(run.latency_ms_p95) + ' ms delay'].filter(Boolean).join(' · '), { dimColor: true, wrap: 'wrap' })
      : text('No analysis is running on this camera.', { dimColor: true });
    const warning = status.warning && text(status.warning, { color: status.color, wrap: 'wrap' });

    // 3. The next step: what to do, how to ask Claude, and the one action that moves it forward.
    const step = guide(s, candidate, state);
    const action = busy ? undefined
      : step.action === 'run' && s.camera ? Button({ key: 'run', label: 'Run on camera', hotkey: 'r', variant: 'primary',
        onPress: () => act($, 'review', { action: 'apply', digest: candidate.digest }, true) })
      : step.action === 'prepare' ? Button({ key: 'prepare', label: 'Test on camera', hotkey: 't', variant: 'primary',
        onPress: () => act($, 'prepare', {}) })
      : undefined;
    const nextStep = card([
      column([text(step.title, { weight: 'semibold', color: step.color }), step.message && text(step.message, { dimColor: true, wrap: 'wrap' })]),
      (action || (step.cancel && s.canCancel)) && row([action,
        step.cancel && s.canCancel && Button({ key: 'cancel-job', label: 'Cancel test', variant: 'secondary', onPress: () => act($, 'cancel', {}) })]),
      step.starters && !busy && row(step.starters.map(([label, request]) =>
        Button({ key: 'starter-' + label, label, variant: 'secondary', onPress: () => suggest($, request) }))),
    ], step.color || 'subtle');

    // 4. Quiet controls for what is already on the camera.
    const controls = row([
      !busy && state.active && Button({ key: 'stop', label: 'Stop analysis', plain: true, dimColor: true,
        onPress: () => act($, 'review', { action: 'stop' }, true) }),
      !busy && state.previous && Button({ key: 'rollback', label: 'Go back to previous', plain: true, dimColor: true,
        onPress: () => act($, 'review', { action: 'rollback' }, true) }),
      larger(Svg ? 'Larger view' : 'Open live video'),
    ]);

    return column([
      header,
      column([preview, caption, warning]),
      alert,
      nextStep,
      controls,
      toggle(),
      technical([['Camera', cam?.name], ['Camera path', s.camera],
        ['Running', state.active ? `${current.name} · ${state.active.digest}` : 'none'],
        ['Previous', state.previous ? `${describe(s, state.previous.digest, state.previous.name).name} · ${state.previous.digest}` : 'none'],
        ['Project revision', s.candidate?.digest || 'none'],
        ['Operation', `${s.operation.state}: ${s.operation.message}`], ['Workspace', s.workspace]]),
    ], 1);
  });
}
