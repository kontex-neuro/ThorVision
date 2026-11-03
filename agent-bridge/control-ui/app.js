/* Human controls for any agent. Mirrors the Claude Code panel: camera and health, the live
   result, one next step, quiet camera controls. Every camera change goes through a reviewed
   confirmation bound to an exact revision. No agent turn is needed for controls. */
const $ = (id) => document.getElementById(id);
const query = new URLSearchParams(location.search);
const fragment = location.hash.slice(1);
if (fragment) { sessionStorage.setItem('thorvision-token', fragment); history.replaceState(null, '', location.pathname + location.search); }
const token = fragment || sessionStorage.getItem('thorvision-token') || '';
if (/^(light|dark)$/.test(query.get('theme') || '')) document.documentElement.dataset.theme = query.get('theme');
// A host may name the agent driving this page; otherwise copy stays agent-neutral.
const agentName = (query.get('agent') || '').replace(/[^\p{L}\p{N} ._-]/gu, '').trim().slice(0, 24);
const agent = agentName || 'your agent';
const Agent = agentName || 'Your agent';
const possessive = agentName ? agentName + '’s' : 'your agent’s';

let snapshot, offline = { title: 'Connecting…', message: 'Starting the ThorVision service. This page retries by itself.' };
let pending, locked = false, notice = '', frameUrl, lastFrame = 0, videoSince;
let cameraSignature = '', packageSignature = '', startersSignature = '';

class ApiError extends Error { constructor(message, status) { super(message); this.status = status; } }
async function api(path, body) {
  const response = await fetch('/api/' + path, { method: body === undefined ? 'GET' : 'POST',
    headers: { Authorization: 'Bearer ' + token, ...(body === undefined ? {} : { 'Content-Type': 'application/json' }) },
    body: body === undefined ? undefined : JSON.stringify(body), signal: AbortSignal.timeout(20000) });
  const result = await response.json().catch(() => ({}));
  if (!response.ok) throw new ApiError(result.error || `The device answered ${response.status}.`, response.status);
  return result;
}
async function act(work) {
  if (locked) return;
  locked = true; notice = ''; render();
  try { await work(); await refresh(); } catch (e) { notice = e.message; }
  finally { locked = false; render(); }
}
async function review(body) { pending = await api('review', body); }

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
function describe(digest, fallback) {
  const item = snapshot?.packages.find((p) => p.digest === digest);
  const when = ago(item?.report?.time);
  return { name: item?.manifest?.name || fallback || 'Analysis', note: [when && 'tested ' + when, short(digest)].filter(Boolean).join(' · ') };
}
const STARTERS = [
  ['Track the animal', 'Track the mouse in the arena. Draw its position and its path over the last few seconds on the video, and save its position for every frame so I can analyse it later.'],
  ['Recognise behaviours', 'Recognise when the mouse is grooming, rearing, walking or resting. Show the current behaviour on the video and log when each behaviour starts and ends.'],
  ['Count zone entries', 'Count how often the mouse enters a zone I describe, for example the left third of the arena. Show the count on the video and log the time of each entry.'],
];
const CHANGE = ['Change the analysis', 'Change the analysis running on the camera so that '];

// Is processing healthy? Problems are always named, never left for the user to infer.
function health(s, state) {
  const run = state.run, fault = state.fault || run?.fault;
  if (fault) return { word: 'Fault', tone: 'error', warning: 'The analysis reported a fault: ' + (fault.message || JSON.stringify(fault)) };
  if (!state.active) return { word: 'Idle', tone: 'subtle' };
  if (run?.state === 'starting') return { word: 'Starting…', tone: 'warning' };
  if (run?.state !== 'running') return { word: 'Not running', tone: 'error', warning: 'The analysis is installed but not processing video' + (run?.state ? ` (status: ${run.state}).` : '.') };
  // Health follows the video this page actually shows. A page that has just opened has no
  // frame yet; that is not a stall until it lasts.
  if (lastFrame) videoSince = undefined; else videoSince ??= Date.now();
  if (!lastFrame && Date.now() - videoSince < 10000) return { word: 'Connecting video…', tone: 'subtle' };
  if (Date.now() - lastFrame > 5000) return { word: 'Video paused', tone: 'warning', warning: 'No new video for a few seconds. The camera or the analysis may have stalled.' };
  if (run.below_target) return { word: 'Slower than target', tone: 'warning',
    warning: `Processing is below its target of ${run.target_fps} frames per second. The caption shows the current rate.` };
  return { word: 'Live', tone: 'success' };
}
// The next step: one card, chosen from backend facts. Readiness stays the backend's decision.
function guide(s, candidate, state) {
  const op = s.operation?.state;
  if (s.canCancel || op === 'preparing' || op === 'cancelling') return { title: 'Testing on the camera…', tone: 'busy', message: s.operation.message, cancel: true };
  if (op === 'running-operation') return { title: 'Switching the camera…', tone: 'busy', message: s.operation.message };
  if (s.authoring) return { title: `${Agent} is working on your request…`, tone: 'busy', message: 'When the change is finished, it is tested on the camera automatically.' };
  const digest = s.candidate?.digest;
  const fresh = s.hasProject && digest && (s.candidate.dirty || (digest !== state.active?.digest && digest !== state.previous?.digest));
  if (op === 'failed' && (fresh || !candidate?.ready)) return { title: 'That didn’t work', tone: 'error',
    message: s.operation.message + ' The camera keeps running what it ran before.',
    action: candidate?.ready && !s.candidate?.dirty ? 'run' : 'prepare',
    starters: [[`Ask ${agent} to fix it`, `The last test on the camera failed with this error: ${s.operation.message} Please fix the analysis and test it again.`]] };
  if (fresh && candidate?.ready && !s.candidate.dirty) return { title: 'New analysis ready', tone: 'suggestion',
    message: `${describe(digest, s.candidate.name).name} passed a test run on ${candidate.report?.frames ?? 'the'} sample frames. Run it, then check the result in the live video.`, action: 'run' };
  if (fresh) return { title: 'New analysis not tested yet', message: op === 'cancelled' ? s.operation.message : 'Test it on the camera before running it.', action: 'prepare' };
  if (state.active) return { title: 'What would you like to change?', message: `Pick a starting point to copy, or describe the change in ${possessive} chat.`, starters: [CHANGE, ...STARTERS] };
  return { title: `Tell ${agent} what to analyse`, message: `Pick a starting point. It is copied for you to paste into ${possessive} chat, edit and send.`, starters: STARTERS };
}

// Task starters: this page cannot type into an agent's chat, so it copies the request.
async function copy(text) {
  try { await navigator.clipboard.writeText(text); return true; }
  catch {
    const area = Object.assign(document.createElement('textarea'), { value: text });
    area.style.cssText = 'position:fixed;opacity:0'; document.body.append(area); area.select();
    const done = document.execCommand('copy'); area.remove(); return done;
  }
}
const COPY_ICON = '<svg viewBox="0 0 16 16" aria-hidden="true"><rect x="5.5" y="5.5" width="8" height="8" rx="1.5"/><path d="M10.5 3.5v-.5A1.5 1.5 0 0 0 9 1.5H4A1.5 1.5 0 0 0 2.5 3v5A1.5 1.5 0 0 0 4 9.5h.5"/></svg>';
const CHECK_ICON = '<svg viewBox="0 0 16 16" aria-hidden="true"><path d="M3 8.5l3 3 7-7"/></svg>';
// The confirmation lives on the starter itself, so nothing covers the video or the controls.
function starterButton([label, request], index, all) {
  const button = document.createElement('button');
  const show = (icon, text) => { button.innerHTML = icon; button.append(text); };
  show(COPY_ICON, label);
  if (all[0] === CHANGE && index === 0) button.className = 'lead';
  button.onclick = async () => {
    const done = await copy(request);
    // Embedded hosts often block the clipboard: then the request is shown, selected, to copy by hand.
    $('copy-fallback').hidden = done;
    if (!done) { $('copy-text').value = request; $('copy-text').focus(); $('copy-text').select(); return; }
    show(CHECK_ICON, `Copied. Paste it into ${possessive} chat`); button.classList.add('copied');
    $('copy-status').textContent = `Copied. Paste it into ${possessive} chat, edit it, then send.`;
    clearTimeout(button.reset);
    button.reset = setTimeout(() => { show(COPY_ICON, label); button.classList.remove('copied'); $('copy-status').textContent = ''; }, 4000);
  };
  return button;
}
const NEW_LINK = [`Ask ${agent} for a new link`, 'Open the ThorVision panel again and give me the new link.'];
// Screen readers hear a status only when it changes.
function say(id, value) { if ($(id).textContent !== value) $(id).textContent = value; }

function terms(target, pairs) {
  target.replaceChildren(...pairs.map(([label, value, className]) => {
    const wrap = document.createElement('div');
    const dt = document.createElement('dt'); dt.textContent = label;
    const dd = document.createElement('dd'); dd.textContent = String(value ?? '—'); if (className) dd.className = className;
    wrap.append(dt, dd); return wrap;
  }));
}
function plan(pairs) {
  $('confirm-plan').replaceChildren(...pairs.flatMap(([label, value, className]) => {
    const dt = document.createElement('dt'); dt.textContent = label;
    const dd = document.createElement('dd'); dd.textContent = value; if (className) dd.className = className;
    return [dt, dd];
  }));
  $('confirm-plan').hidden = !pairs.length;
}

// Confirmation: one focused card in plain sentences, until the user answers.
function renderConfirm(s, state, where) {
  const showing = !$('confirm').hidden;
  if (pending.action === 'cleanup') {
    const p = pending.plan || {};
    $('confirm-title').textContent = 'Remove these from the device?';
    $('confirm-message').textContent = p.blocked?.length
      ? 'Some of these are protected, so the device will not remove anything. Cancel and change the selection.'
      : 'Only the items in this device plan are removed. This cannot be undone.';
    plan([['Analyses', String(p.packages?.length ?? 0)], ...(p.include_unused_cache ? [['Cache', 'Unused dependency cache']] : []),
      ...(Number.isFinite(p.total_bytes) ? [['Frees', `${(p.total_bytes / 1048576).toFixed(1)} MiB`]] : []),
      ...(p.blocked?.length ? [['Protected', `${p.blocked.length} item${p.blocked.length > 1 ? 's' : ''}`, 'danger']] : [])]);
    $('confirm-yes').textContent = 'Remove';
    terms($('confirm-details'), [['Device plan', JSON.stringify(p, null, 2)]]);
  } else {
    const target = pending.action === 'stop' ? undefined : describe(pending.digest, s.candidate?.name);
    const now = pending.activeDigest ? describe(pending.activeDigest, state.active?.name) : undefined;
    const tested = s.packages.find((p) => p.digest === pending.digest)?.report;
    // Same-named analyses are told apart by when they were tested.
    const label = (item) => item.name + (target && now && target.name === now.name ? ` (${item.note})` : '');
    const copy = {
      apply: [`Run ${target && target.name !== s.candidate?.name ? label(target) : 'the new analysis'} on ${where}?`, 'Run on camera', [
        now ? `It replaces ${label(now)}. You can go back to it afterwards.` : 'Nothing is running on this camera now.',
        tested?.passed && `It passed a test run on ${tested.frames} sample frames. Check the result in the live video.`]],
      rollback: [`Go back to the previous analysis on ${where}?`, 'Go back', [
        `${target ? label(target) : 'The previous analysis'} replaces ${now ? label(now) : 'what is running now'}. You can switch again afterwards.`]],
      stop: [`Stop the analysis on ${where}?`, 'Stop analysis', [
        `${now?.name || 'The running analysis'} stops. It stays on the device, so you can run it again later.`]],
    }[pending.action];
    $('confirm-title').textContent = copy[0];
    $('confirm-message').textContent = copy[2].filter(Boolean).join(' ');
    $('confirm-yes').textContent = copy[1];
    plan([]);
    terms($('confirm-details'), [['Camera path', pending.camera], ['Exact revision', pending.digest || 'none'],
      ['Replaces', pending.activeDigest || 'none'], ['Camera revision', pending.revision]]);
  }
  $('confirm-yes').disabled = locked || !!pending.plan?.blocked?.length;
  $('confirm').hidden = false;
  if (!showing) { $('confirm').querySelector('details').open = false; $('confirm-no').focus(); }
}
function cancelConfirm() { pending = undefined; $('confirm').hidden = true; render(); }

function render() {
  const view = !snapshot ? (offline.error ? 'offline' : 'connecting') : pending ? 'confirm' : 'main';
  document.body.dataset.view = view;
  $('alert').textContent = notice ? 'That didn’t go through: ' + notice : ''; $('alert').hidden = !notice;
  if (!snapshot) {
    $('offline').hidden = false; $('confirm').hidden = true;
    $('camera-name').textContent = 'ThorVision'; $('camera').hidden = true;
    say('health', offline.expired ? 'Link expired' : offline.error ? 'Offline' : 'Connecting…'); $('health').dataset.tone = offline.error ? 'error' : 'subtle';
    $('offline-title').textContent = offline.title; $('offline-message').textContent = offline.message;
    $('offline-details').hidden = !offline.error; $('offline-error').textContent = offline.error || '';
    // Reconnecting cannot revive a rejected link; the one way forward is asking for a new one.
    $('reconnect').hidden = !offline.error || offline.expired;
    $('offline-starters').hidden = !offline.expired;
    if (offline.expired && !$('offline-starters').childElementCount) $('offline-starters').append(starterButton(NEW_LINK, 0, [NEW_LINK]));
    $('offline').append($('copy-fallback'));
    return;
  }
  $('offline').hidden = true;
  $('step').append($('copy-fallback'));
  const s = snapshot, state = s.state.cameras[s.camera] || {}, busy = locked || s.busy || s.authoring;
  const cam = s.cameras.find((c) => c.id === s.camera), where = cam ? cameraName(cam.name) : 'this camera';
  const candidate = s.packages.find((p) => p.digest === s.candidate?.digest);
  const current = state.active && describe(state.active.digest, state.active.name);
  const status = health(s, state);

  // 1. Camera and its health
  const cs = JSON.stringify(s.cameras.map((c) => [c.id, c.name]));
  if (cs !== cameraSignature) { $('camera').replaceChildren(...s.cameras.map((c) => new Option(cameraName(c.name), c.id))); cameraSignature = cs; }
  const choose = s.cameras.length > 1;
  $('camera').hidden = !choose; $('camera').value = s.camera || ''; $('camera').disabled = busy || !!pending;
  $('camera-name').hidden = choose; $('camera-name').textContent = cam ? cameraName(cam.name) : 'No camera selected';
  document.title = `${cam ? cameraName(cam.name) : 'Camera'} · ThorVision`;
  say('health', locked ? 'Working…' : status.word); $('health').dataset.tone = locked ? 'warning' : status.tone;

  // 2. The result
  const run = state.run?.state === 'running' ? state.run : undefined;
  $('caption').textContent = current
    ? [current.name, run?.processed_fps != null && run.processed_fps.toFixed(1) + ' FPS', run?.latency_ms_p95 != null && Math.round(run.latency_ms_p95) + ' ms delay'].filter(Boolean).join(' · ')
    : 'No analysis is running on this camera.';
  say('warning', status.warning || ''); $('warning').hidden = !status.warning; $('warning').dataset.tone = status.tone;
  $('frame').alt = `Live processed video from ${where}`;
  $('empty').textContent = state.active ? 'Waiting for video…' : 'The live video appears here once an analysis runs.';

  if (pending && !busy) renderConfirm(s, state, where);
  else if (pending && locked) $('confirm-yes').disabled = true;
  else $('confirm').hidden = true;

  // 3. The next step
  const step = guide(s, candidate, state);
  $('step').dataset.tone = step.tone || 'subtle';
  $('step-title').textContent = step.title; $('step-message').textContent = step.message || ''; $('step-message').hidden = !step.message;
  const runnable = !busy && step.action === 'run' && !!s.camera, testable = !busy && step.action === 'prepare', cancellable = step.cancel && s.canCancel;
  $('run').hidden = !runnable; $('prepare').hidden = !testable; $('cancel-job').hidden = !cancellable; $('cancel-job').disabled = locked;
  $('prepare').textContent = step.tone === 'error' ? 'Test again' : 'Test on camera';
  $('step-actions').hidden = !(runnable || testable || cancellable);
  const starters = !busy && step.starters || [];
  const ss = JSON.stringify(starters);
  if (ss !== startersSignature) { $('starters').replaceChildren(...starters.map(starterButton)); startersSignature = ss; $('copy-fallback').hidden = true; }
  $('starters').hidden = !starters.length;

  // 4. Quiet controls for what is already on the camera
  $('stop').hidden = busy || !state.active; $('rollback').hidden = busy || !state.previous;
  $('controls').hidden = $('stop').hidden && $('rollback').hidden;

  const ps = JSON.stringify([s.packages.map((p) => [p.digest, p.ready, p.can_delete, p.protected_reasons, p.report?.time]), s.candidate?.digest]);
  if (ps !== packageSignature) {
    const selected = $('package').value;
    $('package').replaceChildren(new Option('Choose an analysis', ''), ...s.packages.filter((p) => p.ready).map((p) => {
      const d = describe(p.digest); return new Option(`${d.name} · ${d.note}`, p.digest);
    }));
    $('package').value = s.packages.some((p) => p.digest === selected && p.ready) ? selected : '';
    $('packages').replaceChildren(...s.packages.map((p) => {
      const label = document.createElement('label'); label.className = 'check';
      const input = Object.assign(document.createElement('input'), { type: 'checkbox', value: p.digest, disabled: !p.can_delete });
      const d = describe(p.digest), name = document.createElement('span'); name.textContent = d.name;
      const detail = document.createElement('small');
      detail.textContent = d.note + (p.protected_reasons?.length ? ' · protected: ' + p.protected_reasons.join(', ') : '');
      name.append(detail); label.append(input, name); return label;
    }));
    packageSignature = ps;
  }
  const chosen = s.packages.find((p) => p.digest === $('package').value);
  $('package').disabled = busy;
  $('readiness').textContent = chosen ? `Passed a test run on ${chosen.report?.frames ?? '—'} sample frames.` : 'Only analyses that passed a test run are listed.';
  $('run-other').disabled = busy || !chosen?.ready || !s.camera;
  $('cleanup').disabled = busy;
  terms($('tech'), [['Camera', cam?.name], ['Camera path', s.camera],
    ['Running', state.active ? `${current.name} · ${state.active.digest}` : 'none'],
    ['Previous', state.previous ? `${describe(state.previous.digest, state.previous.name).name} · ${state.previous.digest}` : 'none'],
    ['Project revision', s.candidate?.digest || 'none'], ['Operation', `${s.operation?.state}: ${s.operation?.message}`],
    ['Memory', run?.worker_rss_bytes == null ? '—' : `${Math.round(run.worker_rss_bytes / 1048576)} MiB`],
    ['Latency p95', run?.latency_ms_p95 == null ? '—' : `${Math.round(run.latency_ms_p95)} ms`],
    ['Workspace', s.workspace]]);
}

async function refresh() {
  try { snapshot = await api('status'); offline = {}; }
  catch (e) {
    snapshot = undefined;
    offline = !token || e.status === 401
      ? { title: 'This link has expired', message: `Ask ${agent} to open the ThorVision panel again. It gives you a fresh link.`, error: e.message || 'No access token in the link.', expired: true }
      : { title: 'Not connected', message: 'The ThorVision service did not answer. Check that the bridge is running and the camera is reachable. This page retries every few seconds, or press Reconnect.', error: e.message };
  }
  render();
}
// An expired link cannot recover by itself; an unreachable device is retried more slowly.
let statusTimer;
async function pollStatus() {
  clearTimeout(statusTimer); await refresh();
  if (!offline.expired) statusTimer = setTimeout(pollStatus, snapshot ? 1500 : 5000);
}
async function pollFrame() {
  try {
    if (snapshot) {
      const response = await fetch('/api/frame', { headers: { Authorization: 'Bearer ' + token }, signal: AbortSignal.timeout(8000) });
      if (response.ok && response.status !== 204) {
        const time = Number(response.headers.get('X-Frame-Time'));
        if (time !== lastFrame) {
          const url = URL.createObjectURL(await response.blob()); $('frame').src = url;
          if (frameUrl) URL.revokeObjectURL(frameUrl); frameUrl = url; lastFrame = time;
        }
      }
    }
  } catch {} finally {
    const fresh = Date.now() - lastFrame < 5000;
    $('frame').hidden = !fresh; $('empty').hidden = fresh;
    setTimeout(pollFrame, 200);
  }
}

$('reconnect').onclick = () => { offline = { title: 'Connecting…', message: 'Reaching the ThorVision service.' }; render(); pollStatus(); };
$('camera').onchange = () => act(async () => { pending = undefined; await api('camera', { camera: $('camera').value }); });
$('package').onchange = () => render();
$('prepare').onclick = () => act(() => api('prepare', {}));
$('cancel-job').onclick = () => act(() => api('cancel', {}));
$('run').onclick = () => act(() => review({ action: 'apply', digest: snapshot.candidate.digest }));
$('run-other').onclick = () => act(() => review({ action: 'apply', digest: $('package').value }));
$('stop').onclick = () => act(() => review({ action: 'stop' }));
$('rollback').onclick = () => act(() => review({ action: 'rollback' }));
$('cleanup').onclick = () => act(async () => { pending = await api('cleanup/review', {
  digests: [...$('packages').querySelectorAll('input:checked')].map((input) => input.value), includeUnusedCache: $('cache').checked }); });
$('confirm-no').onclick = cancelConfirm;
$('confirm-yes').onclick = () => act(async () => { const id = pending?.id; pending = undefined; if (id) await api('execute', { id }); });
document.addEventListener('keydown', (event) => { if (event.key === 'Escape' && pending && !locked) cancelConfirm(); });
pollStatus(); pollFrame();
