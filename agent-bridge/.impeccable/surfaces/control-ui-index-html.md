---
version: 1
slug: "control-ui-index-html"
primary_target: "control-ui/index.html"
related_targets: ["control-ui/app.js","control-ui/style.css"]
---

# ThorVision browser view

Scope: `control-ui/` (index.html, style.css, app.js). Mode: Operate.

Audience and job: a scientist beside a camera, with any agent's chat open next to this page. They need to know what to do next, see the live processed video, and approve camera changes.

Constraints: works equally as a 360–480 px embedded side panel and a full tab; agent-neutral copy (`?agent=` names the agent); starters copy a request; same controls and reviewed confirmation as the Claude Code mod; no build step.

## Direction contract

THESIS: The browser twin of the Claude Code panel: one calm column that answers "what now?" under the live video. It refuses the dashboard of equal-weight cards, metrics tiles and a permanently visible draft block.

OWN-WORLD: Claude Code desktop's panel language. Warm neutral ground, hairline-bordered rounded cards, system UI type at panel sizes, one filled ink primary button, quiet text buttons for secondary controls. Colour only carries state: green live, amber attention, red fault, clay for a ready change.

STORY: The user sees which camera and whether it is healthy, sees the result, reads one next step with one action or starters to copy into their agent, and confirms changes in a separate view.

FIRST VIEWPORT: Narrow: header (camera name, health word), video at full width, caption line, next-step card, quiet controls, disclosures. Wide (≥ 900 px): header across; video takes the left ~62 %; the right column holds the next step, controls and disclosures. Confirmation replaces the right column (narrow: everything below the header).

FORM: Pinned by the brief (mirror the mod), position 1 of 1; the roll was recorded but not used, because the user pinned the structure. seed:3c998f65aa458d20

FINISH: unreviewed and undocumented is unfinished; this build ends with the finish review, the verdict, DESIGN.md, and every shipping raster carrying its provenance
