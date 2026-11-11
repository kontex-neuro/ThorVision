// The native agent owns chat and authentication. This mod owns human controls.
let connection, snapshot, pending, timer, frameTimer, polling = false, acting = false, notice = '', connectionNotice = '';
let active = false, details = false, editing = false, frame, framePolling = false, frameUnsupported = false, videoSince;
let earlierDigest = '';
let localDemos;
let operatorOutputTarget;
let operatorConversationActive = false, operatorConnectionIdentity;
const SERVER = 'plugin:thorvision-framelab:framelab';
const unavailable = (reason) => `ThorVision editing is unavailable for this turn: ${reason} Do not modify the analytics extension project or call prepare_extension this turn. Answer normally, and if the user asks for analytics changes, tell them this reason and to check /framelab.`;
const GUIDE = `This is a ThorVision FrameLab authoring session. Use the ThorVision MCP tools to inspect the SDK, device capabilities and existing project before editing. Write only the extension project. Preserve original frame association and camera isolation. Use real processing; never substitute simulated detections. Finish changes with prepare_extension: it validates, reuses compatible dependencies automatically, installs through the temporary PC proxy when needed, and tests the exact candidate. Report actual errors and measured performance. The mod also checks preparation when your turn finishes. The developer pre-deploys and maintains the service; all user operations use its API. Do not ask the user to connect over SSH, request reuse or run validation manually. If the API is unavailable, report its error for the developer; do not deploy the service or fall back to SSH. Activation, rollback and cleanup belong to the human ThorVision controls; never call the local control HTTP actions or activate through shell commands. Tell the user to open /framelab and click Run on camera for a tested candidate. Device tests use fixture frames, not a detection-accuracy benchmark.`;

function rememberDemos(value) {
  const options = value?.options;
  if (value?.schema !== 'framelab.demo-catalog/1' || !Array.isArray(options) || options.length !== 2) return;
  if (new Set(options.map((item) => item?.id)).size !== 2 || !options.every((item) =>
    ['person-detector', 'three-zone-entries'].includes(item?.id) && typeof item.label === 'string' && item.label &&
    typeof item.prompt === 'string' && item.prompt && /^[a-f0-9]{64}$/.test(item.digest || ''))) return;
  localDemos = value;
}

function demoRequests(text) {
  const paragraphs = text.split(/\r?\n/).map((line) => line.trim());
  return localDemos?.options.filter((item) => paragraphs.includes(item.prompt.trim())) || [];
}

function demoContext(demo) {
  return `This request selects the built-in ${demo.id} demo. Reproduce its frozen implementation automatically; the user-facing request deliberately omits installation details. Call get_demo_recipes, then restore_demo_recipe with id "${demo.id}". The expected package digest is ${demo.digest} and configuration is exactly {}. Do not generate, rewrite, or edit implementation code. Verify the returned identity and complete frozen recipe at recipe_directory, with the separate callback copy at package_directory. Inspect the target SDK/capabilities, call prepare_extension, and compare the actual package, model, dependency and configuration identities with the frozen recipe. Report environment drift or a failed verification rather than changing pinned identities or substituting regenerated code. If other text requests changes to the frozen implementation, explain that difference and clarify before editing. Preserve any running analysis. Camera activation, stopping, rollback and cleanup remain human-reviewed controls. After the exact recipe passes, open the control panel for the user to review Run on camera; never activate it yourself.`;
}

const DIO_GUIDE = 'This is direct operator-requested digital I/O on the selected FrameLab device, separate from camera callback authoring. Use get_digital_io and set_digital_outputs only for this I/O action; do not create or edit a callback, begin an authoring operation, call prepare_extension, finalize a camera project, or activate/stop an analysis. Read get_digital_io first and follow its authoritative compatibility, ready/freshness and valid-mask result. For a printed box connector, require the current qualified protocol-4 box mapping or a separately verified operator mapping; legacy logical protocol support does not establish box wiring. The current box exposes OUT1–4 (bits 0–3); internal channels 5–8 are physically unqualified. Digital OUT2 is the external TTL connector, not camera-status RGB LED2/GPIO20. Send one masked write only for the operator-requested action and preserve all other outputs. Require the tool\'s matched confirmation; an uncertain HIGH must never be replayed automatically. On conflict, read fresh status and report it; do not silently fetch a revision and resend. A later explicit LOW/Stop is a new action even after uncertain HIGH. If fresh status already confirms the requested LOW, report it without an unnecessary write. Until I say stop means leave the requested latch set, not an agent polling loop, pulse or unrequested timeout. MCP disconnect does not clear it while SPI polling continues; polling-loss watchdog protection is not an agent-session lease. Report confirmed FPGA latch readback separately from the operator\'s visual LED observation; never claim the LED is lit or off from an acknowledgment alone. No SSH, direct SPI/HTTP writes, second publisher or callback hardware/network/MCP access.';

function digitalIORequest(text) {
  const message = text.trim();
  if (/^(?:please\s+)?(?:stop|turn\s+(?:it|that)\s+off|switch\s+(?:it|that)\s+off|stop\s+(?:it|that))(?:\s+(?:now|please))?[.!]?$/i.test(message)) {
    return operatorConversationActive && operatorOutputTarget
      ? { kind: 'stop', mask: operatorOutputTarget, values: 0 }
      : { kind: 'clarify', reason: 'No prior explicit digital-output control target is known. Ask whether the user means a particular digital output or the camera analysis; do not infer a channel or actuate anything.' };
  }
  const channels = [...message.matchAll(/\b(?:digital\s*)?out(?:put)?(?:\s+channel)?\s*([1-8])\b/gi)].map((match) => Number(match[1]));
  const mentionsIO = channels.length || /\b(?:digital\s*(?:i\s*\/\s*o|inputs?|outputs?)|dio|tdio|get_digital_io|set_digital_outputs)\b/i.test(message)
    || /\bout(?:put)?\s*\d+\b/i.test(message);
  const otherLed = /\bgpio\s*20\b|\b(?:camera|status|rgb)[^.!?\n]{0,40}\bled\s*2\b/i.test(message);
  if (otherLed || (!mentionsIO && /\bled\s*2\b/i.test(message)))
    return { kind: 'clarify', reason: 'Camera-status RGB LED2/GPIO20 is not the external digital OUT2 TTL connector. Explain the distinction and clarify the requested hardware; do not call set_digital_outputs or prepare a camera package for it.' };
  if (!mentionsIO) return;
  if (/\b(if|when|whenever|while|automatically|callback|processor|python|detect|detects|detection|track|tracking|overlay|analysis)\b/i.test(message)
      || /\b(each|every)\s+time\b/i.test(message))
    return { kind: 'clarify', reason: 'This mixes camera/callback work or conditional automation with hardware control. Clarify the independent operator action first. Camera callbacks stay isolated and cannot call MCP, network, SPI or digital-output tools; do not implement hardware access inside setup/process or run an automatic output loop.' };
  if (channels.some((channel) => channel > 4) || /\bout(?:put)?\s*(?:0|[9]|\d{2,})\b/i.test(message)
      || /\bout(?:put)?\s*\d\s*[-–]\s*\d/i.test(message))
    return { kind: 'clarify', reason: 'Confirm a qualified box connector OUT1–4 before changing outputs. Channels 5–8 are internal and physically unqualified; do not invent a connector or interpret a range as one bit.' };
  const control = /\b(turn|switch|set|drive|make|clear|raise|lower|toggle|blink|enable|disable)\b|\bset_digital_outputs\b/i.test(message);
  if (!control) return { kind: 'read' };
  const high = /\b(on|high|raise|enable)\b/i.test(message), low = /\b(off|low|clear|lower|disable)\b/i.test(message);
  if (!channels.length || high === low || /\b(toggle|blink)\b/i.test(message))
    return { kind: 'clarify', reason: 'Ask which qualified digital output and explicit HIGH or LOW action the operator wants. Do not guess a channel, toggle, blink or change outputs while the request is ambiguous.' };
  const mask = channels.reduce((value, channel) => value | (1 << (channel - 1)), 0);
  return { kind: 'write', mask, values: high ? mask : 0 };
}

function digitalIOContext(request) {
  if (request.kind === 'clarify') return request.reason;
  if (request.kind === 'read') return DIO_GUIDE + ' This request is read-only: inspect get_digital_io and report its state; do not write outputs or replace the remembered operator control target.';
  const target = `The requested output mask is ${request.mask}; the explicit set_digital_outputs arguments are {"mask":${request.mask},"values":${request.values}}. Preserve every bit outside this mask.`;
  return DIO_GUIDE + ' ' + target + (request.kind === 'stop'
    ? ' This short stop refers to the last explicit operator output target only if the conversation still confirms that target. If intervening context makes it ambiguous, ask which output/action to stop before writing. Read fresh status, then clear only this mask; never stop the camera analysis.'
    : ' Remember this explicit output target for the operator\'s later Stop; a status read alone does not choose or replace it.');
}

function rememberOperatorConnection(value) {
  if (typeof value?.api !== 'string' || typeof value?.token !== 'string') throw new Error('The selected FrameLab MCP connection could not be identified.');
  const identity = value.api + '\0' + value.token;
  if (operatorConnectionIdentity && operatorConnectionIdentity !== identity) {
    operatorOutputTarget = undefined; operatorConversationActive = false;
    frame = snapshot = undefined;
  }
  operatorConnectionIdentity = identity;
}

async function refreshOperatorConnection($) {
  const result = await $.mcp.call(SERVER, 'open_control_panel', {});
  if (result.isError) throw new Error('The selected FrameLab MCP connection is unavailable.');
  const current = JSON.parse(result.content.find((item) => item.type === 'text').text);
  rememberOperatorConnection(current);
  connection = current;
}

async function connect($) {
  if (connection) return;
  const result = await $.mcp.call(SERVER, 'open_control_panel', {});
  if (result.isError) throw new Error(result.content?.[0]?.text || 'ThorVision MCP is unavailable.');
  connection = JSON.parse(result.content.find((item) => item.type === 'text').text);
  rememberOperatorConnection(connection);
  // This is local MCP data and remains usable when the camera API is offline.
  // Standard runtimes do not expose the optional tool.
  try {
    const demos = await $.mcp.call(SERVER, 'get_demo_recipes', {});
    if (!demos.isError) rememberDemos(JSON.parse(demos.content.find((item) => item.type === 'text').text));
  } catch {}
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
    rememberDemos(snapshot.demo);
    if (!snapshot.packages.some((item) => item.digest === earlierDigest && item.ready)) earlierDigest = '';
    connectionNotice = '';
  }
  catch (error) { snapshot = undefined; connectionNotice = error.message; }
  finally { polling = false; $.ui.invalidate('ui.render'); }
}
const refused = () => !connection && /refused|classifier|denied/i.test(connectionNotice);
// The panel preview: one camera or processed frame about every second. An older bridge has no
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
async function reviewDisconnected($, camera) {
  if (acting) return;
  acting = true; notice = ''; $.ui.invalidate('ui.render');
  try {
    let reviewed;
    try { reviewed = await api($, 'disconnected/review', { camera }); }
    catch (error) {
      if (error.message !== 'Unknown operation') throw error;
      const selected = snapshot.cameras.find((item) => item.discovered && item.id === snapshot.camera)?.id;
      let switched = false;
      try {
        await api($, 'camera', { camera }); switched = true;
        reviewed = await api($, 'review', { action: 'stop' });
      } finally {
        if (switched && selected && selected !== camera) await api($, 'camera', { camera: selected });
      }
    }
    if (reviewed?.action !== 'stop' || reviewed.camera !== camera) throw new Error('The camera changed while reviewing. Review the disconnected analysis again.');
    pending = reviewed; details = false;
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
  if (!state.active) {
    const raw = s.state?.raw_previews?.[s.camera];
    if (raw?.fault) return { word: 'Camera unavailable', color: 'error', warning: raw.fault.message || String(raw.fault) };
    if (s.preview && Date.now() - s.preview.receivedAt < 5000) return { word: 'Camera live', color: 'success' };
    videoSince ??= Date.now();
    return Date.now() - videoSince < 10000
      ? { word: 'Connecting camera…', color: 'subtle' }
      : { word: 'Camera unavailable', color: 'warning', warning: 'No new camera video. Check that the camera is connected and streaming.' };
  }
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
function disconnectedBlocker(s, state) {
  if (state.active || s.operation?.state !== 'failed' || !s.operation.message?.includes('analytics is already active on ')) return;
  return s.cameras.find((item) => item.discovered === false && item.state?.active && s.operation.message.includes(item.id));
}
function guide(s, candidate, state) {
  const op = s.operation?.state;
  if (s.canCancel || op === 'preparing' || op === 'cancelling') return { title: 'Testing on the camera…', message: s.operation.message, cancel: true };
  if (op === 'running-operation') return { title: 'Switching the camera…', message: s.operation.message };
  if (s.authoring) return { title: 'Claude is working on your request…', message: 'When Claude finishes a change, it is tested on the camera automatically.' };
  if (disconnectedBlocker(s, state)) return { title: 'Stop the disconnected analysis first', color: 'warning', blocked: true,
    message: 'An analysis is still assigned to a camera that is no longer detected. Stop it under “Analyses on disconnected cameras” below, then run your analysis on the connected camera.' };
  const digest = s.candidate?.digest;
  const fresh = s.hasProject && digest && (s.candidate.dirty || (digest !== state.active?.digest && digest !== state.previous?.digest));
  if (op === 'failed' && (fresh || !candidate?.ready)) return { title: 'That didn’t work', color: 'error',
    message: s.operation.message + ' The camera keeps running what it ran before.',
    action: candidate?.ready && !s.candidate?.dirty ? 'run' : 'prepare',
    starters: [['Ask Claude to fix it', `The last test on the camera failed with this error: ${s.operation.message} Please fix the analysis and test it again.`]] };
  if (fresh && candidate?.ready && !s.candidate.dirty) return { title: 'New analysis ready', color: 'suggestion',
    message: `${describe(s, digest, s.candidate.name).name} passed a test run on ${candidate.report?.frames ?? 'the'} sample frames. Run it, then check the result in the live preview.`, action: 'run' };
  if (fresh) return { title: 'New analysis not tested yet', message: op === 'cancelled' ? s.operation.message : 'Test it on the camera before running it.', action: 'prepare' };
  const demos = s.demo?.options?.map((item) => [item.label, item.prompt]);
  if (state.active) return { title: demos ? 'Choose a demo' : 'What would you like to change?', message: 'Add to what runs now, or describe a change or a different analysis in Claude.', starters: demos || [CHANGE, ...FOLLOW_UPS] };
  return { title: demos ? 'Choose a demo' : 'Tell Claude what to analyse', message: 'Pick a starting point. It goes into Claude’s message box for you to edit and send.', starters: demos || STARTERS };
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
    operatorOutputTarget = undefined; operatorConversationActive = false; operatorConnectionIdentity = undefined;
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
    if (active && connection && (editing || snapshot?.authoring)) { try { await api($, 'authoring/finish', { aborted: true }); } catch {} }
    pending = undefined;
    active = false;
    operatorOutputTarget = undefined;
    operatorConversationActive = false; operatorConnectionIdentity = undefined;
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
    let io = digitalIORequest(e.text);
    if (io) {
      try { await refreshOperatorConnection($); io = digitalIORequest(e.text); }
      catch { operatorOutputTarget = undefined; operatorConversationActive = false;
        return next({ ...e, context: [...(e.context || []), 'The selected digital-I/O device connection could not be confirmed. Report that connection error and clarify the target before changing any output. Do not prepare or activate a camera analysis.'] }); }
      if (io.kind === 'write') { operatorOutputTarget = io.mask; operatorConversationActive = true; }
      if (io.kind === 'clarify') operatorConversationActive = false;
      return next({ ...e, context: [...(e.context || []), digitalIOContext(io)] });
    }
    operatorConversationActive = false;
    const demos = demoRequests(e.text);
    if (demos.length > 1) return next({ ...e, context: [...(e.context || []),
      'This message contains both built-in demo requests. Ask which demo the user wants before proceeding. Do not select, restore, edit, prepare or activate either demo until the choice is clear.'] });
    // Never drop the prompt: a dropped prompt looks like Claude stopped answering.
    try { await api($, 'authoring/begin', {}); editing = true; }
    catch (error) { return next({ ...e, context: [...(e.context || []), unavailable(error.message)] }); }
    return next({ ...e, context: [...(e.context || []), GUIDE, ...(demos.length ? [demoContext(demos[0])] : [])] });
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
      localDemos && card([
        text('Choose a demo', { weight: 'semibold' }),
        text('The recipes are available locally. Connect the camera device before preparing or running an analysis.', { dimColor: true, wrap: 'wrap' }),
        row(localDemos.options.map((item) => Button({ key: 'starter-' + item.label, label: item.label,
          variant: 'secondary', onPress: () => suggest($, item.prompt) }))),
      ]),
      !starting && toggle(),
      !starting && technical([['Error', connectionNotice]]),
    ], 1);

    const s = snapshot, busy = acting || s.busy || s.authoring;
    const detected = s.cameras.filter((item) => item.discovered);
    const cam = detected.find((item) => item.id === s.camera);
    const state = cam ? s.state.cameras[cam.id] || {} : {};
    const where = cam ? cameraName(cam.name) : 'this camera';
    const candidate = s.packages.find((item) => item.digest === s.candidate?.digest);
    const current = state.active && describe(s, state.active.digest, state.active.name);
    const blocker = disconnectedBlocker(s, state);
    const status = blocker ? { word: 'Blocked', color: 'warning' } : health(s, state);
    const alert = notice && text('That didn’t go through: ' + notice, { color: 'error', wrap: 'wrap' });

    // 1. Camera and its health in one word.
    const header = column([
      Box({ flexDirection: 'row', flexWrap: 'wrap', justifyContent: 'space-between', columnGap: 'md', children: [
        text(cam ? cameraName(cam.name) : 'No camera selected', { weight: 'semibold', size: 'heading' }),
        text(acting ? 'Working…' : status.word, { color: acting ? 'warning' : status.color, weight: 'medium' })] }),
      detected.length > 1 && !busy && !pending ? Select({ key: 'camera', label: 'Camera', value: s.camera,
        options: detected.map((item) => ({ value: item.id, label: cameraName(item.name) })),
        onSelect: (camera) => { pending = undefined; frame = undefined; return act($, 'camera', { camera }); } })
        : !cam && text('Choose a camera in the larger view.', { dimColor: true }),
    ]);

    // Confirmation: one focused card in plain sentences, until the user answers.
    if (pending && !busy) {
      const reviewed = s.cameras.find((item) => item.id === pending.camera);
      const reviewedState = s.state.cameras[pending.camera] || state;
      const reviewWhere = reviewed?.discovered ? cameraName(reviewed.name) : 'the disconnected camera';
      const target = pending.action === 'stop' ? undefined : describe(s, pending.digest, s.candidate?.name);
      const now = pending.activeDigest ? describe(s, pending.activeDigest, reviewedState.active?.name) : undefined;
      const tested = s.packages.find((p) => p.digest === pending.digest)?.report;
      // Same-named analyses are told apart by when they were tested.
      const label = (item) => item.name + (target && now && target.name === now.name ? ` (${item.note})` : '');
      const copy = {
        apply: () => [`Run the new analysis on ${reviewWhere}?`, 'Run on camera', [
          now ? `It replaces ${label(now)}. You can go back to it afterwards.` : 'Nothing is running on this camera now.',
          tested?.passed && `It passed a test run on ${tested.frames} sample frames. Check the result in the live preview.`]],
        rollback: () => [`Go back to the previous analysis on ${reviewWhere}?`, 'Go back', [
          `${label(target)} replaces ${now ? label(now) : 'what is running now'}. You can switch again afterwards.`]],
        stop: () => [`Stop the analysis on ${reviewWhere}?`, 'Stop analysis', [
          `${now?.name || 'The running analysis'} stops. It stays on the device, so you can run it again later.`]],
      }[pending.action]();
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

    // 2. Camera video is available before an analysis is prepared or running.
    const svg = Svg && frame && `<svg xmlns="http://www.w3.org/2000/svg" width="${frame.width || 640}" height="${frame.height || 480}" viewBox="0 0 ${frame.width || 640} ${frame.height || 480}"><image href="data:image/jpeg;base64,${frame.jpeg}" width="100%" height="100%"/></svg>`;
    const preview = !Svg ? undefined
      : svg && svg.length <= 131072 ? Svg({ source: svg, alt: `Live camera video from ${where}` })
      : card([text(frameUnsupported ? 'Restart Claude to see the live video here.' : svg ? 'Open the larger view to see this camera image.' : 'Connecting to the camera…', { dimColor: true, wrap: 'wrap' })]);
    const run = state.run?.state === 'running' ? state.run : undefined;
    const rawPreview = frame?.kind === 'raw' || s.preview?.kind === 'raw' || !state.active;
    const caption = rawPreview
      ? text('Live camera preview · no analysis is running on this image.', { dimColor: true, wrap: 'wrap' })
      : current
      ? text([current.name, run?.processed_fps != null && run.processed_fps.toFixed(1) + ' FPS',
        run?.latency_ms_p95 != null && Math.round(run.latency_ms_p95) + ' ms delay'].filter(Boolean).join(' · '), { dimColor: true, wrap: 'wrap' })
      : text('No analysis is running on this camera.', { dimColor: true });
    const warning = status.warning && text(status.warning, { color: status.color, wrap: 'wrap' });

    // 3. The next step: what to do, how to ask Claude, and the one action that moves it forward.
    const step = !cam ? { title: s.cameraDiscoveryError ? 'Camera discovery unavailable' : detected.length ? 'Choose a camera' : 'No camera detected',
      message: s.cameraDiscoveryError ? 'FrameLab could not read the connected cameras. Check the device connection and try again.' : detected.length ? 'Choose a connected camera above.' : 'Connect a camera to continue.' } : guide(s, candidate, state);
    const action = busy ? undefined
      : step.action === 'run' && cam ? Button({ key: 'run', label: 'Run on camera', hotkey: 'r', variant: 'primary',
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
    const disconnected = s.cameras.filter((item) => item.discovered === false && item.state?.active);
    const disconnectedControls = disconnected.length && card([
      text('Analyses on disconnected cameras', { weight: 'semibold' }),
      text('These analyses are saved against cameras that are no longer detected.', { dimColor: true, wrap: 'wrap' }),
      !busy && row(disconnected.map((item) => Button({ key: 'stop-disconnected-' + item.id,
        label: 'Stop ' + describe(s, item.state.active.digest, item.state.active.name).name,
        variant: 'secondary', onPress: () => reviewDisconnected($, item.id) }))),
    ]);
    const readyPackages = s.packages.filter((item) => item.ready && (!s.demo || s.demo.options.some((option) => option.digest === item.digest)));
    const chosen = readyPackages.find((item) => item.digest === earlierDigest);
    const canRunEarlier = !busy && !!cam && !!chosen && !blocker;
    const earlierAnalyses = card([
      text('Run an earlier analysis', { weight: 'semibold' }),
      readyPackages.length && !busy ? Select({ key: 'package', label: 'Analysis', value: earlierDigest,
        options: [{ value: '', label: 'Choose an analysis' }, ...readyPackages.map((item) => {
          const description = describe(s, item.digest);
          return { value: item.digest, label: `${description.name} · ${description.note}` };
        })],
        onSelect: (digest) => { if (!busy) { earlierDigest = digest; $.ui.invalidate('ui.render'); } } })
        : text(!readyPackages.length ? 'No tested analyses are available yet.' : chosen ? describe(s, chosen.digest).name : 'Wait for the current operation to finish before choosing an analysis.', { dimColor: true, wrap: 'wrap' }),
      text(chosen ? `Passed a test run on ${chosen.report?.frames ?? '—'} sample frames.` : 'Only analyses that passed a test run are listed.', { dimColor: true, wrap: 'wrap' }),
      canRunEarlier && Button({ key: 'run-other', label: 'Review run', variant: 'secondary',
        onPress: () => canRunEarlier && act($, 'review', { action: 'apply', digest: chosen.digest }, true) }),
    ]);

    return column([
      header,
      column([preview, caption, warning]),
      alert,
      nextStep,
      controls,
      disconnectedControls,
      earlierAnalyses,
      toggle(),
      technical([['Camera', cam?.name], ['Camera path', s.camera],
        ['Running', state.active ? `${current.name} · ${state.active.digest}` : 'none'],
        ['Previous', state.previous ? `${describe(s, state.previous.digest, state.previous.name).name} · ${state.previous.digest}` : 'none'],
        ['Project revision', s.candidate?.digest || 'none'],
        ['Operation', `${s.operation.state}: ${s.operation.message}`], ['Workspace', s.workspace]]),
    ], 1);
  });
}
