---
name: ThorVision
description: The browser twin of the Claude Code panel for a camera's live analysis. Warm neutrals, hairline cards, and colour only for state.
colors:
  ground: "#faf9f5"
  surface: "#ffffff"
  sunken: "#f2f0ea"
  ink: "#1f1e1d"
  muted: "#65635e"
  line: "#e6e3db"
  line-strong: "#cfcbc0"
  primary: "#1f1e1d"
  on-primary: "#faf9f5"
  primary-hover: "#3a3936"
  clay: "#b0502c"
  clay-line: "#e2b4a1"
  success: "#2c7a4b"
  warning: "#8f5b06"
  warning-line: "#e5c98f"
  error: "#b3261e"
  error-line: "#ecb3ad"
  stage: "#1b1b1a"
  on-stage: "#c9c6bd"
  ground-dark: "#262624"
  surface-dark: "#2f2f2d"
  sunken-dark: "#2a2a28"
  ink-dark: "#f0eee6"
  muted-dark: "#a8a59c"
  line-dark: "#3b3b38"
  line-strong-dark: "#55544f"
  primary-hover-dark: "#d9d6cc"
  clay-dark: "#e3916f"
  clay-line-dark: "#7a4733"
  success-dark: "#73c493"
  warning-dark: "#e2ac52"
  warning-line-dark: "#6e5628"
  error-dark: "#f08b80"
  error-line-dark: "#7a3a34"
  stage-dark: "#151514"
typography:
  title-lg:
    fontFamily: "system-ui, -apple-system, Segoe UI, sans-serif"
    fontSize: "16px"
    fontWeight: 600
    lineHeight: 1.3
  title:
    fontFamily: "system-ui, -apple-system, Segoe UI, sans-serif"
    fontSize: "15px"
    fontWeight: 600
    lineHeight: 1.35
  body:
    fontFamily: "system-ui, -apple-system, Segoe UI, sans-serif"
    fontSize: "14px"
    fontWeight: 400
    lineHeight: 1.5
  body-strong:
    fontFamily: "system-ui, -apple-system, Segoe UI, sans-serif"
    fontSize: "14px"
    fontWeight: 500
    lineHeight: 1.5
  label:
    fontFamily: "system-ui, -apple-system, Segoe UI, sans-serif"
    fontSize: "12px"
    fontWeight: 400
    lineHeight: 1.5
  mono:
    fontFamily: "ui-monospace, Cascadia Mono, Consolas, monospace"
    fontSize: "12px"
    fontWeight: 400
    lineHeight: 1.5
rounded:
  sm: "6px"
  md: "8px"
  lg: "10px"
  full: "50%"
spacing:
  2xs: "4px"
  xs: "6px"
  sm: "8px"
  md: "12px"
  lg: "16px"
  xl: "24px"
components:
  button-primary:
    backgroundColor: "{colors.primary}"
    textColor: "{colors.on-primary}"
    typography: "{typography.body-strong}"
    rounded: "{rounded.md}"
    padding: "6px 12px"
    height: "36px"
  button-primary-hover:
    backgroundColor: "{colors.primary-hover}"
  button-secondary:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.ink}"
    typography: "{typography.body-strong}"
    rounded: "{rounded.md}"
    padding: "6px 12px"
    height: "36px"
  button-secondary-hover:
    backgroundColor: "{colors.sunken}"
  button-quiet:
    backgroundColor: "transparent"
    textColor: "{colors.muted}"
    typography: "{typography.body-strong}"
    rounded: "{rounded.md}"
    padding: "6px 8px"
    height: "36px"
  button-quiet-hover:
    backgroundColor: "{colors.sunken}"
    textColor: "{colors.ink}"
  select:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.ink}"
    typography: "{typography.body}"
    rounded: "{rounded.md}"
    padding: "6px 12px"
    height: "36px"
  textarea:
    backgroundColor: "{colors.sunken}"
    textColor: "{colors.ink}"
    typography: "{typography.body}"
    rounded: "{rounded.md}"
    padding: "8px 10px"
  card:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.ink}"
    rounded: "{rounded.lg}"
    padding: "14px 16px"
  stage:
    backgroundColor: "{colors.stage}"
    textColor: "{colors.on-stage}"
    rounded: "{rounded.lg}"
  header-bar:
    backgroundColor: "{colors.ground}"
    textColor: "{colors.ink}"
    typography: "{typography.title-lg}"
    padding: "12px 16px"
---

# Design System: ThorVision

## Overview

**Creative North Star: "The Twin Panel"**

ThorVision's browser view is a twin of the Claude Code desktop panel, set beside a scientist's agent chat. It reads as part of the host tool rather than a product with its own brand: a warm off-white (or warm charcoal) ground, white hairline-bordered cards with gently rounded corners, and system UI type at panel sizes. Most of the page is neutral. The live video sits on a near-black stage and is the most visually prominent thing on the page.

Density is calm and vertical. The page works first as a 360 to 480 px side panel and opens out into two columns in a full tab. Each card has one clear action. Secondary controls are quiet text buttons, and the machinery (earlier analyses, storage, technical details) sits behind chevron disclosures divided by hairlines. Colour is used only for state: green means live, amber means attention, red means fault, and clay marks a change that is ready to run. Clay also marks keyboard focus.

Both themes are first-class. The light theme is the default. The dark theme follows `prefers-color-scheme` unless `data-theme` forces one or the other. Every role has a paired dark value.

**Key Characteristics:**
- Warm neutral ground with white hairline cards. Depth comes from tone and 1px lines, never from shadows.
- System UI type from 12 to 16 px. Hierarchy comes from weight and muted colour, not from size jumps.
- One filled ink button per card. Outlined secondary buttons and borderless quiet buttons carry everything else.
- Colour is reserved for state (success, warning, error, clay suggestion) and for focus.
- A near-black video stage with rounded corners, the same in both themes.
- Disclosures with CSS chevrons and hairline separators hold the technical detail.

## Colors

A warm, low-chroma neutral system with four state hues. No hue is decorative.

### Primary
- **Panel Ink** (`primary`): the fill of the one primary action in a card ("Run on camera", "Test on camera", "Reconnect"). It is the same value as body ink, so the primary button reads as dense text, not as a brand colour. Hover lightens it to `primary-hover`. In dark theme the button inverts to light ink on charcoal (`ink-dark` fill, `ground-dark` text).

### Secondary
- **Clay** (`clay`, dark `clay-dark`): the "ready change" signal. It colours the title of a suggestion-tone step card, the busy spinner's arc, the focus outline, the checkbox `accent-color`, and the text-selection tint (28% mix). **Clay Line** (`clay-line`) is the border of suggestion cards and the confirmation card.

### Tertiary (state)
- **Live Green** (`success`): the health word and dot when processing is live, and a "Copied" starter's text and border.
- **Attention Amber** (`warning`): the health word while working, and stale or degraded warnings under the video. `warning-line` is its paired border.
- **Fault Red** (`error`): offline or expired health, error-tone step titles, alerts, and destructive plan values. `error-line` is its paired card border.

### Neutral
- **Warm Paper** (`ground`): the page and the sticky header bar.
- **Card White** (`surface`): cards, outlined buttons, selects.
- **Sunken Linen** (`sunken`): hover fill for outlined and quiet buttons, and the read-only copy textarea.
- **Ink** (`ink`): body text and titles.
- **Muted Stone** (`muted`): secondary text, captions, disclosure summaries at rest, `dt` labels, quiet buttons at rest.
- **Hairline** (`line`): card borders, the header's bottom rule, disclosure separators.
- **Strong Hairline** (`line-strong`): control borders (buttons, selects, textarea), the spinner track, the scrollbar thumb.
- **Stage Black** (`stage`, dark `stage-dark`) with **Stage Text** (`on-stage`): the video stage and its empty-state message.

### Named Rules
**The State-Only Colour Rule.** A hue appears only when it carries state (live, attention, fault, ready change) or focus. Everything else uses the warm neutrals. If removing a colour would lose no information, the colour should not be there.

**The Paired Line Rule.** Every state that tints a card's border has its own low-contrast `-line` partner (`clay-line`, `warning-line`, `error-line`). A full-strength state hue is never used as a card border.

**The Dark Stage Rule.** The video stage stays near-black in both themes. Processed video is judged against a dark surround, so the stage never follows the page ground.

## Typography

**Body Font:** system-ui (with -apple-system, Segoe UI, sans-serif)
**Label/Mono Font:** ui-monospace (with Cascadia Mono, Consolas, monospace)

**Character:** The host platform's own UI face at panel sizes, so the page sits inside its host instead of announcing a brand. Monospace is kept for machine values only.

### Hierarchy
- **Title Large** (600, 16px, 1.3): the camera name in the header bar and the confirmation card's title.
- **Title** (600, 15px, 1.35, balanced wrap): card titles, such as the next-step heading.
- **Body** (400, 14px, 1.5): messages and captions. Prose in cards is capped at 68ch. Captions use tabular numerals for FPS and delay.
- **Body Strong** (500, 14px): buttons, the health word, disclosure summaries, plan values. Primary buttons step up to 600.
- **Label** (400, 12px, 1.5): `dt` labels in technical lists and checkbox sub-lines, in muted colour.
- **Mono** (400, 12px, 1.5): revisions, paths, and error output inside Technical details only.

### Named Rules
**The Panel Size Rule.** Type runs from 12 to 16 px, and there is no display size. Hierarchy comes from weight (600 / 500 / 400) and from ink versus muted colour. On this page the live video carries the visual hierarchy.

**The Machinery In Mono Rule.** Monospace appears only behind a disclosure. A digest or path shown in the main reading flow is a sign that machinery has leaked into the plain-language layer.

## Layout

The page has a sticky header bar on the ground colour, separated from the content by a hairline. The header holds the camera name (or a camera select capped at 320px) and the health word, and it wraps on narrow widths. Under 900px the content is a single column with 16px padding and a 16px gap: video stage, caption line, then the side stack of cards, quiet controls and disclosures. The side stack uses a 12px gap.

At 900px and above the layout becomes two columns: a fluid video column and a side column clamped between 320 and 400px. The gap grows to 24px, the padding to 20px 24px 32px, and the maximum width is 1440px. The video column stays pinned 72px from the top, and the stage height is capped at the viewport minus 150px. Single-card views (offline, connecting) narrow to 560px and one column. The confirmation view replaces the side column on wide screens and everything below the header on narrow ones.

Spacing is in 4px steps, and the values in use are 4, 6, 8, 12, 16 and 24px. Card padding is 14px by 16px. Rows inside a card use 12px gaps, button groups use 8px, and starter rows use 6px.

## Elevation & Depth

The system is flat. Depth is tonal: the white `surface` sits one step above the warm `ground`, and a 1px hairline completes the edge. The `sunken` tone gives the hover and inset layer. No shipped element uses a shadow, including cards, the confirmation card, buttons and the sticky header. Emphasis comes from border colour (the paired `-line` tokens) and from a short entrance motion (the confirmation card rises 6px over 0.2s on `cubic-bezier(.16, 1, .3, 1)`).

### Named Rules
**The Hairline Rule.** Structure is drawn with 1px warm lines (`line` for containers, `line-strong` for controls). Cards are flat at rest and stay flat when emphasised. Emphasis changes the border colour, not the elevation.

## Shapes

Corners are gently rounded, and the radius depends on scale. Large containers (cards and the video stage) use 10px. Controls (buttons, selects, textarea) use 8px. Disclosure summaries use 6px so the focus outline follows a soft corner. Status dots and the busy spinner are full circles. Borders are always 1px. The only exception is the spinner's 1.5px stroke, which matches the chevron's 1.5px stroke. Chevrons are drawn in CSS as a rotated two-sided border. Icons are inline 16×16 stroke SVGs: 15px rendered, 1.6 stroke, round caps and joins, at 70% opacity.

## Components

### Buttons
Dense, text-led buttons. They read as panel controls, not as marketing calls to action.
- **Shape:** gently rounded (8px), minimum height 36px, padding 6px 12px, 7px gap to an optional leading icon.
- **Primary:** an ink fill with ground-coloured text, weight 600. Use at most one per card.
- **Secondary (default):** a white surface with a strong hairline border and ink text at weight 500. Hover fills it with `sunken`.
- **Quiet:** transparent with a transparent border, muted text, 8px horizontal padding. Hover shows ink text on `sunken`. Used for camera controls outside cards ("Stop analysis", "Go back to previous").
- **Hover / Focus:** background, border and colour change over 0.15s ease-out. Focus is a 2px clay outline at a 2px offset. Disabled buttons drop to 45% opacity with a not-allowed cursor.

### Starters
A vertical stack of full-width secondary buttons, left-aligned, each with a copy icon. Clicking one copies an editable request for the agent. On success the row turns green (`success` text and border), the icon switches to a check at full opacity, and the label changes to "Copied…" for 4s. When a starter changes the running analysis, it leads the stack with 6px of extra space below it, to separate it from new tasks.

### Cards / Containers
- **Corner Style:** 10px.
- **Background:** `surface` on `ground`.
- **Shadow Strategy:** none (see Elevation & Depth).
- **Border:** 1px `line`. Suggestion-tone and confirmation cards use `clay-line`, and error-tone cards use `error-line`, with the title in the matching state hue.
- **Internal Padding:** 14px 16px, with a 12px column gap.
- **Busy state:** the title gets a 12px ring spinner (`line-strong` track, clay arc, 0.9s linear) before the text. This is the only spinner on the page.

### Inputs / Fields
- **Select:** styled like a secondary button (white, strong hairline, 8px radius, 36px high), full width, with ellipsis overflow.
- **Textarea (copy fallback):** `sunken` fill, strong hairline, 8px radius, 8px by 10px padding, vertical resize, read-only.
- **Checkbox:** native, 16px, tinted by `accent-color: clay`. The label is a flex row with a muted 12px sub-line.
- **Focus:** the shared 2px clay outline.

### Navigation
There is no navigation chrome. The sticky header bar acts as the page's anchor: the camera name in Title Large on the left, and the health indicator on the right.

### Health Indicator
A word with a 7px dot before it in `currentColor`, weight 500, no wrapping. It is muted when idle or connecting and takes the tone colour for success, warning, error or suggestion. The word itself carries the state, and the dot repeats it.

### Disclosure
A native `details` with a hairline top border. The summary is a 36px-high muted row (weight 500) with a CSS chevron that rotates from pointing right to pointing down over 0.15s. Hover and open turn the summary to ink. Stacked disclosures in the side column share a single top rule. Content inside uses 10px vertical rhythm.

### Video Stage
A 4:3 near-black frame with 10px corners and `object-fit: contain`. While empty, it centres a 34ch message in `on-stage`. Under it sits a single muted caption line (analysis name, FPS, delay), plus a warning line in amber or red only when needed.

## Do's and Don'ts

### Do:
- **Do** take every colour from the custom properties on `:root`, and give every new role a dark-theme value in both the `prefers-color-scheme` block and the `[data-theme="dark"]` block.
- **Do** keep one filled ink primary per card, and use the outlined default or quiet buttons for everything else.
- **Do** mark emphasis on a card with its paired `-line` border and a state-coloured title, not with fill or elevation.
- **Do** keep type between 12 and 16px in system UI, separating levels by weight (600 / 500 / 400) and muted colour.
- **Do** draw icons as inline stroke SVGs (16×16 viewBox, 1.6 stroke, round caps, 70% opacity) and chevrons as CSS borders.
- **Do** keep controls at least 36px high with the 2px clay focus outline at a 2px offset.
- **Do** honour `prefers-reduced-motion`: every transition and animation turns off.

### Don't:
- **Don't** use a hue for decoration. Clay, green, amber and red appear only when they carry state or focus.
- **Don't** put a full-strength state hue on a card border. Use its `-line` partner.
- **Don't** add shadows to cards, the header, or the confirmation card. Shipped depth is tone plus hairline.
- **Don't** lighten the video stage to match the page in light theme.
- **Don't** show monospace machine values (revisions, hashes, paths) outside a Technical details disclosure.
- **Don't** use icon fonts, emoji, or text glyphs as icons.
