# ThorVision FrameLab Control design

## Purpose and surface

Operate mode. A camera developer describes processing in native Claude Code,
then reviews a tested candidate, runs it and watches real results. The native
conversation remains the authoring surface. No second chat, SSH form or account
form is introduced.

## Direction chosen before implementation

The camera image is the primary evidence. A compact native pane owns camera,
candidate, preparation progress and explicit run controls. A companion browser
view dedicates most space to the processed frame, with the same controls in a
right column. This avoids squeezing live video into terminal cells. Package
history and cleanup are disclosed below the main task, not separate dashboards.

Native pane: inherit Claude's type, spacing and semantic colors. Browser:
Segoe UI/system sans, 14/16/20/28px scale, 8px spacing rhythm. Paper #f4f6f8,
surface #ffffff, ink #172b3a, muted #536575, action #155eaa, success #236844,
danger #a82432. A dark camera stage prevents bright empty frames. No decorative
gradients, KPI tiles, fake camera images or generated metrics.

```
Native conversation          ThorVision panel
Describe the processing      Camera [KonteX-XC-U3]
Agent edits and prepares     Active: person tracking
                            Candidate: Ready
                            [Run on camera]
                            [Stop] [Roll back]
                            Open live view

Browser live view                         Controls
Large real processed camera image        Camera / connection
Frame identity + freshness               Candidate / Prepare / Run
                                         Inline action review
FPS / latency / memory                   Packages (disclosure)
```

## Workflow and states

Empty: explain what to prompt in Claude. Draft: automatically prepare at a
successful turn boundary after a content change. Preparing: one operation,
observable progress, no duplicate starts. Ready: exact digest matched against
device readiness; Run on camera becomes available. Review: camera, package and
effect shown inline; Confirm run is a separate click. Running: preview freshness
and measured runtime values. Failed: exact error, previous active package retained,
no automatic retry. Disconnected: preserve selection and expose retry.

Camera changes clear pending action reviews. A new revision invalidates the
previous candidate's readiness. Reviewing an operation is not authorization;
only the subsequent explicit UI action executes it. Cleanup shows exact items,
protected packages and estimated bytes before deletion.

## Quality and scope

One primary action, plain sentence case, selectable output, keyboard focus,
native controls, 44px browser targets, responsive stack below 800px. Errors and
status are announced; preview is not a screen-reader announcement loop.
No mutation is performed on page load or by a status poll. Controls share the
same device-operation core. The MCP tools exposed to the model do not activate
or delete. Host lifecycle hooks improve automatic preparation; backend readiness
checks remain authoritative if the hook is absent or interrupted.

Design review: a full duplicate analytics dashboard was rejected because it
competes with native chat. Terminal live-video embedding was deferred because
image support differs by Claude surface. This surface follows Impeccable's
Operate guidance and the existing frontend-design skill.
