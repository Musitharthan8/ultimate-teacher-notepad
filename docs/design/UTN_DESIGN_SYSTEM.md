# UTN Design System (GTK3)

Status: derived from code at `origin/feature/utn-teacher-ux-program` (5ac036b1f). This file records what the repo does today. It does not propose values. Where the code defines nothing, the entry says **not yet defined**.

Source files cited throughout:

- `CSS` = `ui/xournalpp.css` (the only UTN theme stylesheet; loaded by `MainWindow::loadMainCSS`, `src/core/gui/MainWindow.cpp`)
- `LAYOUT` = `src/core/gui/ResponsiveLayout.h`
- `MW` = `src/core/gui/MainWindow.cpp`
- `UW` = `src/core/gui/toolbarMenubar/UtnWidgets.cpp`
- `CB` = `src/core/gui/toolbarMenubar/ToolUtnContextBar.cpp`
- `OLD` = `docs/design/utn-design-system.md` (earlier prose doc; see "Differences from the earlier doc")

## 1. GTK3 constraints

- GTK 3.24 CSS has no custom properties, `var()`, flexbox or `gap`. Tokens are `@define-color` names (CSS lines 13-60). Light and dark are separate names; there is no scoped theming. (CSS header comment, line 28 comment)
- Rules apply only under `window.utnShell`, a style class MW adds when the active toolbar is a teacher layout (MW ~860-870, `utn::isTeacherLayout`). Dialogs are separate windows and do not match it; they use the `.utn-dialog` class instead (CSS "Dialogs are their own windows").
- Theme state is carried by style classes on the main window, not media queries:

| Class | Meaning | Set by |
| --- | --- | --- |
| `utnShell` | teacher-layout chrome active | MW ~866 |
| `darkMode` | dark theme (also forced on by High Contrast) | MW 419, 458-462 |
| `utnHighContrast` | High Contrast theme variant | MW 470-473 |
| `utnTouch` | "touch density" setting (`utn/touchUi`) | MW 168-171; `ToolAppearance.cpp` 94-96 |
| `utnCompact` | window narrower than 1080 px | MW 2272 |
| `utnPresent` | UTN Present layout | MW 871-874 |

- Document rendering (pages, PDF, ink) is not styled by these rules. CSS comment "These rules style application chrome only." The canvas is not themed.

## 2. Colour tokens

All from CSS lines 13-60 unless noted.

### 2.1 Core (original teal shell)

| Token | Value | Use in CSS |
| --- | --- | --- |
| `utn_accent` | `#168b7e` | hover/checked tint (alpha), focus ring on combobox/spinbutton, inspector divider hover |
| `utn_accent_strong` | `#126c63` | brand text, selected swatch border (light), live dot |
| `utn_accent_dark` | `#55d6c2` | dark equivalents of accent; blank-cover button, toast button |
| `utn_danger` | `#b3261e` | destructive action text, Blank state |
| `utn_danger_soft` | `#f2b8b5` | destructive action in dark mode |
| `utn_danger_dark` | `#ff8080` | destructive action in High Contrast |
| `utn_surface` | `#f4f6f8` | menubar, app bar (original), sidebar, inspector host |
| `utn_surface_raised` | `#ffffff` | idle Freeze/Blank buttons |
| `utn_surface_rail` | `#edf1f5` | rail (original) |
| `utn_surface_dark` | `#20252c` | dark surface |
| `utn_surface_raised_dark` | `#2a3039` | dark raised |
| `utn_surface_rail_dark` | `#272e36` | dark rail |
| `utn_border` | `#d5dce4` | panel edges, inspector divider |
| `utn_border_soft` | `#e4e9ef` | secondary toolbar bottom edge |
| `utn_border_dark` | `#3a424d` | dark border |

### 2.2 Paper-and-ink (stage 2 shell, wins over 2.1 where both set a property)

CSS lines 28-46. The "Paper-and-ink" rules come later in the file and so override the original shell at equal specificity (CSS 667+, 1380+).

| Token | Value | Role |
| --- | --- | --- |
| `utn_paper_app` | `#fbfaf7` | window background |
| `utn_paper_surface` | `#ffffff` | app bar, rail card, popovers, docks |
| `utn_paper_desk` | `#ece9e2` | workspace switch trough, stroke preview, rail hover base |
| `utn_paper_hairline` | `#e3dfd6` | borders |
| `utn_paper_text` | `#1b2a2e` | text |
| `utn_paper_muted` | `#56666a` | secondary text, idle rail icons |
| `utn_paper_accent` | `#0f766e` | selected tool, width label, Present tint |
| `utn_paper_accent_soft` | `#d9efea` | selected-tool fill |
| `utn_paper_ring` | `#0b4f4a` | focus ring |
| `utn_paper_dark_app` | `#161b1d` | dark window |
| `utn_paper_dark_surface` | `#1e2528` | dark surfaces |
| `utn_paper_dark_desk` | `#101416` | dark trough/preview |
| `utn_paper_dark_hairline` | `#394548` | dark borders |
| `utn_paper_dark_text` | `#e8efed` | dark text |
| `utn_paper_dark_muted` | `#9fb2b4` | dark secondary |
| `utn_paper_dark_accent` | `#4fd1c0` | dark selected |
| `utn_paper_dark_accent_soft` | `#17403b` | dark selected fill |
| `utn_paper_dark_ring` | `#9cf0e4` | dark focus ring |

### 2.3 Semantic status colours

CSS lines 47-56. The comment states each light value keeps 4.5:1 on the paper surface and each dark value on the dark surface. That ratio was not re-measured for this document.

| State | Light | Dark | Class | Applied to |
| --- | --- | --- | --- | --- |
| OK | `#166534` | `#7ee0a0` | `utn-status-ok` | `.utn-document-status` |
| Info | `#0f5f8a` | `#7cc4f0` | `utn-status-info` | same, weight 600 |
| Warning | `#8a5a00` | `#f0c060` | `utn-status-warning` | same, weight 600 |
| Error | `#b3261e` | `#ff8080` | `utn-status-error` | same, weight 700 |

Class names are produced by `src/core/model/DocumentStatus.cpp` (lines 114-119). Severity also changes font weight (ok 400, info/warning 600, error 700) and the status text is words, so colour is not the only signal.

### 2.4 Classroom state colours (Student View)

CSS "Student View status pill" (1047+) and "Classroom quick controls" (1137+). These are literals, not `@define-color` tokens.

| State | Light | Dark | Source |
| --- | --- | --- | --- |
| Live dot | `utn_accent_strong` | `utn_accent_dark` | `.utn-sv-live .utn-sv-dot` |
| Frozen dot / chip | `#b77900`; fill `alpha(#e0a100, .20/.28)` | `#ffc94a`; fill `alpha(#ffc94a,.26)` | `.utn-sv-frozen`, `.utn-quick-freeze:checked` |
| Blank dot / chip | `utn_danger`; checked fill solid `utn_danger`, text `#ffffff` | dot `utn_danger_soft` | `.utn-sv-blank`, `.utn-quick-blank:checked` |
| Off / same | `#7a8590` | not yet defined (no dark override) | `.utn-sv-off`, `.utn-sv-same` |

Literals `#b77900`, `#e0a100`, `#ffc94a`, `#7a8590` are not named tokens: **not yet defined as tokens**.

### 2.5 High Contrast palette

CSS lines 57-60, plus literals.

| Token / literal | Value | Use |
| --- | --- | --- |
| `utn_hc_fg` | `#ffffff` | text, borders |
| `utn_hc_bg` | `#000000` | surfaces |
| `utn_hc_focus` | `#ffff00` | checked/selected, selected-tool text and frame |
| literal | `#00ffff` | keyboard focus outline on buttons (CSS ~"utnHighContrast button:focus") |
| literal | `#333333` | toolbar button hover |
| literal | `#a0a0a0` | disabled text and dashed border |

### 2.6 Other literals (no token)

Toast `#1d2329` bg / `#f2f4f6` text (CSS 1066); blank cover `#000000`/`#ffffff`; "Show page" button text `#0b1f1c`; presentation-banner button `#ffffff`; disabled destructive `#8a8a8a`; legacy floating toolbox `white`, `lightblue`, `black`, `orange` editing border (CSS 40-170). Treat as **not yet defined as tokens**.

## 3. Spacing

**Not yet defined as a scale.** The CSS has no spacing tokens. Values are literal px. Observed in use:

- 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16 px for padding/margin (for example `#tbTop1` padding `6px 8px`; `#tbLeft1` margin `8px 0 8px 8px`; `.utn-context-badge` padding `4px 8px`; `.utn-paper-dock` padding `6px 8px`; toast padding `8px 14px`).
- Popover panel margin: 8 px on all four sides (`UW` `createPopoverPanel`, lines 28-31). Box spacing is a parameter of that function (no default).
- Control-group grid spacing: 2 px between swatches/size buttons in the side panel (`CB` `newControlGroup(..., 2)`, lines 633, 715).
- Colour swatch grid: 3 columns in the side panel (`CB` 633, 708).

The earlier doc's "8 px related / 16 px sections" (OLD "Layout and spacing") is **not found in code**.

## 4. Type

No font family is set; text uses the GTK theme font. Sizes are in `em` (relative) except where noted.

| Role | Size / weight | Source (CSS selector) |
| --- | --- | --- |
| Brand | 1.3em bold | `.utn-brand` |
| Context title (inspector section) | 1.02em, weight 600 (side panel), bold elsewhere | `.utn-context-title` |
| Paper card heading | 15px, 600 | `.utn-paper-heading` |
| Popover title | bold, opacity .8 (1 in High Contrast) | `.utn-popover-title` |
| Document title | 600 | `.utn-document-title` |
| Document status | 0.85em (see 2.3 for weight) | `.utn-document-status` |
| Hint | 0.9em muted (paper); opacity .72 earlier rule, superseded by `@theme_text_color` | `.utn-paper-hint`, `.utn-context-hint` |
| Bottom bar | 0.94em | `#tbBottom1` |
| Rail caption | 12px, weight 500 | `#utnRailLabel`, `#tbLeft1 label` |
| Width label | 600 | `.utn-paper-width-label` |
| Quick controls | 600 | `.utn-sv-quick button` |
| Blank-cover title | 1.6em bold | `.utn-blank-title` |
| Lesson Navigator current page | 700 | `button.utn-current-page` |

Line-height, a type scale, and minimum text size: **not yet defined**.

## 5. Sizes and targets

Mouse density (default) and touch density (`utnTouch`). Effective values after cascade (later rules win):

| Element | Mouse | Touch | CSS selector |
| --- | --- | --- | --- |
| Rail button | min 38x38, padding 4px | min 48x48, padding 7px 8px | `#tbLeft1 button` |
| Rail options corner | 16x16 (18x18 High Contrast) | not yet defined | `.utn-tool-options-corner` |
| Rail tool arrow (classic) | min 24x32 | not yet defined | `.utn-tool-arrow` |
| Rail width | 60 px fixed (`TEACHER_RAIL_WIDTH`, `LAYOUT`); `#utnRailHost` min 60 | same | `LAYOUT` line 21; CSS `#utnRailHost` |
| App bar button | min-height 32, radius 9 | top bars padding 8px vertical | `#tbTop1 button` |
| Context/side button | min-height 32, radius 7 | min-height 46 | `#utnContextSide button` |
| Swatch (side panel) | 24x26 effective (later rule), 34x34 earlier rule | 44x44 | `.utn-swatches button` |
| Swatch dot | 18x18 drawn | same | `UW` line 187 |
| Shared `.utn-swatch` | 26x26, border 2px | 36x36 | `.utn-swatch` |
| Paper palette button | 34x34 | not yet defined | `.utn-paper-palette button` |
| Size buttons | min 44x42 (side), 30 wide (shared) | min-height 48, width 42 | `.utn-sizes button` |
| Ink dock button | 32x32 | 44x44 | `.utn-paper-dock button` |
| Dock glyph button | 28x28 | not yet defined | `.utn-dock-button` |
| Selection toolbar button | min-height 28 | 44x44 | `.utn-selection-toolbar button` |
| Eraser / shape mode button | min-height 32 | 46 / 48 | `.utn-eraser-modes`, `.utn-shape-picker` |
| Context viewport row | min-height 44 | 56 | `#utnContextViewport` |
| Freeze/Blank quick button | min-height 28 | not yet defined | `.utn-sv-quick button` |
| Show-page button on blank cover | min-height 36, padding 8px 28px | n/a | `.utn-blank-show` |
| Inspector splitter | 12 px wide | same | `#utnInspectorPaned separator` |
| Inspector collapse/expand | min-height 32 | n/a | `#utnCollapseInspector` |
| Scrollbars (app bar / rail / context) | 8 px / 6 px / 8 px | same | `#tbTop1Scroller`, `#tbLeft1Scroller`, `#utnContextViewport` |

Minimum hit target for mouse is not uniform (16, 24, 26, 28, 30, 32, 34, 38 px all occur). A single minimum: **not yet defined**.

Inspector preferred width 250 px matches the earlier doc (`MainWindow.h` 318); see 8 for the clamp.

## 6. Radius

Literal px; no tokens.

| Value | Where |
| --- | --- |
| 3-4 px | editing-toolbar border, options corner, scrollbar slider (3) |
| 6-7 px | options corner (paper), context controls (`combobox`, `spinbutton`, `colorbutton` 7), collapse buttons (7) |
| 8 px | toolbar buttons, context badge, shape/eraser buttons, selection toolbar buttons, banner and blank buttons |
| 9 px | app bar buttons, ink dock buttons, stroke preview |
| 10 px | popover.toolbar, rail buttons (original, 10), toast |
| 11 px | rail buttons (effective), workspace switch, pills in the app bar |
| 12 px | paper popover, selection toolbar, legacy floating toolbox |
| 14 px | rail card, class capsule |
| 18 px | ink dock |
| 999 px | swatches, pills, size dots |

## 7. Elevation and borders

- Hairline 1 px border on all paper surfaces (`utn_paper_hairline`); High Contrast uses 2 px `utn_hc_fg` instead and removes shadows.
- Shadows (literal): rail card `0 2px 5px alpha(black,.10)`; popover `0 8px 22px alpha(black,.16)`; dock and selection toolbar `0 4px 14px alpha(black,.16)`; capsule `0 1px 3px alpha(black,.10)`; toast `0 2px 8px alpha(#000,.25)`; workspace switch checked `0 1px 3px alpha(black,.16)`. No elevation token set: **not yet defined**.
- Dark mode does not override these shadows.

## 8. Panels

| Panel | Rule | Source |
| --- | --- | --- |
| Tool rail (`#tbLeft1`) | fixed 60 px card, radius 14, margin `8px 0 8px 8px`, scrolls vertically on short windows (6 px scrollbar) | `LAYOUT` 21; CSS |
| App bar (`#tbTop1`) | 1 row; scrolls horizontally (8 px scrollbar) when too narrow; overflow items move to a "More" menu first | CSS "Design pass 2026-10-30 (responsive)"; `LAYOUT` `appBarItemsToOverflow` |
| Inspector / context panel (`#utnInspectorHost`, `#utnContextSide`) | resizable GtkPaned, wide handle; saved width clamped 150-420 px; effective maximum is `windowWidth - 520` clamped to 150-420, so the canvas keeps at least 520 px; default width 250 px (`inspectorWidth = 250`, `src/core/gui/MainWindow.h` line 318; overridden by saved `utn/inspectorWidth`) | MW 135-138, 1585-1587 |
| Inspector fold | folded to a "Show tool settings" handle below 960 px; returns above 1000 px | `LAYOUT` 83-84 |
| Thumbnail sidebar | hidden below 1400 px; returns above 1440 px; a manual user choice overrides until the next threshold crossing | `LAYOUT` 80-82; MW `updatePanelPolicy` |
| Popovers | `popover.toolbar` radius 10, opaque `@theme_bg_color`; paper popover radius 12 with 8 px content margin | CSS; `UW` 28-31 |
| Ink dock | floating, radius 18, 6px 8px padding | `.utn-paper-dock` |
| Selection toolbar | floating, radius 12, 3px 4px padding | `.utn-selection-toolbar` |
| Toast | `#1d2329` on `#f2f4f6`, radius 10, shown 3500 ms | CSS 1066; `src/core/gui/UtnToast.cpp` line 14 |
| Blank cover | black, white label; has a visible "Show page" way back with 3 px white focus ring | `.utn-blank-cover` |

## 9. Interaction states

Default for rail and app-bar buttons: flat (transparent background and border). State language, light theme, effective:

| State | Rail button | App bar button | Context/side button |
| --- | --- | --- | --- |
| Rest | transparent, icon colour `utn_paper_muted` | transparent | theme default |
| Hover | `shade(utn_paper_desk, 1.04)` fill, icon `utn_paper_text` | same fill | `alpha(utn_accent, .10)` fill (original rule) |
| Checked / active | `utn_paper_accent_soft` fill, `utn_paper_accent` text and border, plus inset 3 px left bar | `alpha(utn_accent, .18)` fill, `alpha(utn_accent,.55)` border | `alpha(utn_accent,.18)` fill, `utn_accent` border (eraser/shape modes) |
| Focus | 2 px ring inside the tile (offset -3 px) | 2 px `utn_paper_ring`, offset 2 px | combobox/spinbutton: `alpha(utn_accent,.70)` 2 px, offset 1 px |
| Disabled | not yet defined (theme default) outside High Contrast and destructive buttons | same | same |
| Destructive | n/a | n/a | tinted red text/border, solid red on hover (`button.destructive-action`) |

Dark: the same states with `*_dark` / `utn_paper_dark_*` tokens (CSS 600-860, 866-990). Dark hover for the original shell uses `alpha(utn_accent_dark, .16)`; dark checked uses `alpha(utn_accent_dark, .28)`.

Transitions or animation: only the legacy floating toolbox (`transition: ... 0.5s ease`, CSS ~64). **Not yet defined** for the UTN shell.

Pressed/active-while-held state distinct from checked: **not yet defined**.

## 10. Themes

Selected by the Settings theme variant (`THEME_VARIANT_USE_SYSTEM`, `FORCE_LIGHT`, `FORCE_DARK`, `HIGH_CONTRAST`; `src/core/control/settings/SettingsEnums.h` 93-96). MW 418-419: High Contrast also sets `darkMode`, so every High Contrast rule sits on top of dark rules and must override them.

### Light
Paper tokens (2.2). Teal accent. Focus ring `#0b4f4a`.

### Dark
`window.utnShell.darkMode` rules. Graphite paper surfaces, light teal accent `#4fd1c0`, focus ring `#9cf0e4`. Pages and PDFs unchanged.

### High Contrast
Black surfaces, white text and 1 px white borders on every button; yellow `#ffff00` for checked/selected; cyan `#00ffff` 2 px focus outline on buttons; disabled is grey `#a0a0a0` with a dashed border; hover inverts to white fill with black text (generic buttons) or `#333333` (toolbar buttons). Selected rail tool keeps a black fill with a 3 px yellow frame so white icons stay visible. Shadows removed. Hints and popover titles forced to opacity 1. Source: CSS "High Contrast" blocks (lines ~330-380, ~560-660, 700-760, 1360-1380 and per-component `utnHighContrast` rules).

## 11. Selected-state cues beyond colour

Code-confirmed non-colour cues:

| Control | Cue | Source |
| --- | --- | --- |
| Selected rail tool | inset 3 px left bar (`box-shadow: inset 3px 0 0`) | CSS `#tbLeft1 button:checked` |
| High Contrast selected rail tool | 3 px frame instead of 1 px | CSS `utnHighContrast #tbLeft1 button:checked` |
| Selected swatch | 3 px selected ring, a tick inside the dot and GTK `CHECKED` state; the ring survives in High Contrast | CSS `.utn-swatch-selected`; `UW::setSwatchSelected` |
| Eraser / shape mode | border-width 2-3 px in High Contrast | CSS |
| Current page in Lesson Navigator | bold 700 text and 4 px left border | CSS `button.utn-current-page`; `ToolLessonNavigator.cpp` 483 |
| Student View state | written label ("Freeze" / "Resume live", "Blank" / "Show page") and a dot glyph | CSS comments; `.utn-sv-dot` |
| Document status | words plus weight step | 2.3 |
| Present workspace | segmented-button label stays explicit | CSS `.utnPresent` comment |
| Disabled (High Contrast, destructive) | dashed border | CSS |
| Inspector divider | colour change on hover/focus only | CSS `#utnInspectorPaned separator:hover` |

Not covered by a non-colour cue in the stylesheet: the original-shell checked state on some app-bar buttons. The Phase 6 palette now has a thicker ring, an interior tick and checked semantics in light, dark and High Contrast themes.

The Document status CSS comment says "a word and a shape as well as a colour". No shape or glyph for document status was found in CSS or `MainWindow.cpp`; only the words and a weight step. The Student View pill does use a dot glyph label (`ToolStudentView.cpp` 738).

## 12. Accessibility rules in code

- Every icon-only control gets an accessible name via `utn::setAccessibleName` (ATK name; `UW` 79-83). Swatches set name and tooltip (`UW` 183-184); options corners set both (`UW` 134-135).
- Hidden captions stay as tooltips and accessible names (`LAYOUT` `railLabelsVisible` doc comment).
- Tooltips can include the keyboard accelerator (`UW::showAcceleratorInTooltip`).
- Keyboard focus is always drawn: 2 px outline in the shell; the rail draws it inside the tile because the rail clips outside outlines (`#tbLeft1 button:focus { outline-offset: -3px }`). Dialog buttons and entries use a two-layer ring: `0 0 0 2px @utn_paper_ring, 0 0 0 4px @utn_paper_surface` so it reads on light and dark (`.utn-dialog`).
- The document status is a real button so keyboard users reach it (`.utn-status-button`, `MW` 938).
- Splitter handle is 12 px wide and takes `:focus` styling; the paned saves width on key release too (MW 158-159).
- Anything covering the app bar leaves a keyboard-focused way back (blank cover "Show page" button, 3 px white focus ring). Statement in `OLD` "Classroom state controls"; behaviour in CSS `.utn-blank-show`.
- Colour is never the only cue for classroom modes (labels) and status (words); see 11.
- High Contrast per-state text colours are set so a selected control cannot show dark text on dark fill (CSS comment above `utnHighContrast button`).
- Contrast targets: 4.5:1 stated in the status colour comment (CSS 47). Ratios for other pairs and a 3:1 focus-ring rule are **not yet defined or verified**.
- Reduced-motion handling: **not yet defined**.
- Text scaling / large font behaviour: **not yet defined** (sizes in `em` follow the GTK font, rail caption is fixed 12px).
- Screen-reader state announcements for toggles beyond GTK defaults: **not yet defined**.

## 13. Narrow-width behaviour

Thresholds are GTK logical pixels (`LAYOUT` header). Each pair has a lower "enter" and higher "leave" value (hysteresis) to stop flicker while dragging the window edge.

| Width | Behaviour | Source |
| --- | --- | --- |
| < 1400 (back > 1440) | thumbnail sidebar hidden | `LAYOUT` 80-82 |
| compact header | `utn-compact-hidden` widgets hidden, except the brand, which stays while width >= 1280 (`setCompactHidden(..., width >= 1280)`) | MW 1939-1950, 2318 |
| < 1080 (back > 1120) | `utnCompact`: icon-only rail captions (Tool labels = Automatic), tighter app-bar padding (4 px sides, buttons `0 4px`), quick controls padding `2px 8px`, pill `2px 6px`, document title hidden (status stays visible), `utn-compact-hidden` widgets hidden, rail button min-width 0 | `LAYOUT` 24-27; MW 2272-2335; CSS 1227+ |
| < 960 (back > 1000) | inspector folded to "Show tool settings" icon button | `LAYOUT` 83-84 |
| any, when app bar too wide | least-important buttons move to "More" menu, in order, until it fits; if still too wide the bar scrolls horizontally | `LAYOUT` `appBarItemsToOverflow`; CSS `#tbTop1Scroller` |
| ignored | allocations under 200 px (placeholder before show) | `LAYOUT` `MIN_MEANINGFUL_WIDTH` |

Tool labels setting: Automatic / Show / Hide (`ToolLabelMode`). Interface mode Beginner = labels shown, Expert = labels hidden and hints removed, Custom = untouched (`LAYOUT` `InterfaceMode`).

Window minimum size, behaviour below 800 px (the CSS comment mentions testing at 800 px), and height-based rules: **not yet defined** in code except the 800 px mention and the rail's vertical scrolling.

## 14. Differences from the earlier doc (`docs/design/utn-design-system.md`)

The earlier doc is prescriptive prose. Where it disagrees with code, code is recorded above. Known mismatches:

| Topic | Earlier doc | Code |
| --- | --- | --- |
| Inspector width | preferred 250, range 190-420 | default 250 matches; range is 150-420, max `width - 520` |
| Accent focus | "cyan focus in High Contrast" | cyan `#00ffff` on buttons only; other widgets use `utn_hc_fg` white outline |
| Selected High Contrast | yellow selected | yellow fill for generic buttons; black fill with yellow text and 3 px yellow frame for the rail |
| Control height | 30-34 mouse, 44-48 touch | many values 24-38 mouse, 36-56 touch (section 5) |
| Spacing 8/16 px | stated | not in code |
| Rail target 38 px | stated | matches (`#tbLeft1 button` min 38) |
| Disabled "patterned/grey" | stated | dashed grey border in High Contrast only |
| Type 1.0-1.05em semibold titles | stated | 1.02em at 600 (inspector); bold elsewhere |
| Palette | teal `#168b7e` accent | still present as `utn_accent`; paper shell uses `#0f766e` on the rail and cards. Two accent families coexist |

## 15. Gaps summary (not yet defined)

1. Spacing scale (all values literal).
2. Named type scale, line-height, minimum text size.
3. Single minimum target size; touch sizes for several controls.
4. Elevation / shadow tokens and dark-mode shadows.
5. Radius tokens.
6. Named tokens for classroom state colours, toast, blank cover and High Contrast greys.
7. Dark values for the Student View "off/same" dot.
8. Pressed state; disabled state outside High Contrast and destructive buttons.
9. Motion tokens and reduced-motion handling.
10. Verified contrast ratios other than the stated 4.5:1 for status colours; focus-ring 3:1.
11. Original-shell checked cues on some app-bar buttons (the Phase 6 palette swatch cue is defined).
12. Window minimum size.
13. Duplicated/overridden rules (for example swatch size 34 then 26, two accent families, rail padding set four times) are unresolved; the effective value is the last rule that matches.
14. Text-scaling behaviour.
15. Dialog styling beyond `.utn-dialog` focus ring.

## 16. Teacher Annotation Palette, Phase 6 WIP (3 November 2026)

Source of truth: `src/core/model/TeacherPalette.h`, `src/core/gui/toolbarMenubar/ToolMenuHandler.cpp`,
`src/core/gui/toolbarMenubar/UtnWidgets.cpp` and `ui/xournalpp.css`. These are **code observations, not Windows verification**.

- **Swatches:** ink tools use Black `#000000`, Blue `#1E5BD8`, Red `#D32F2F`, Green `#2E7D32`,
  Purple `#6A1B9A`, Orange `#EF6C00`, Teal `#00897B` and Pink `#D63384`.
  The Highlighter has Yellow `#FFEB3B`, Pink `#FF7EB6`, Cyan `#00D5E8` and Green `#8BE04E`.
  Its opacity remains the active Highlighter tool's alpha. Custom colours use the existing chooser.
- **Selection:** `.utn-swatch.utn-swatch-selected` has a 3 px ring plus a tick in the colour dot and the GTK checked
  state. The ring is `@utn_accent_strong` in light theme, `@utn_accent_dark` in dark theme and `@utn_hc_fg`
  in High Contrast. No new swatch-token family was created.
- **Existing tokens reused:** `@utn_paper_accent` / `@utn_paper_dark_accent` for active controls,
  `@utn_paper_accent_soft` / `@utn_paper_dark_accent_soft` for checked stroke-tile fills,
  `@utn_paper_desk` / `@utn_paper_dark_desk` for previews, and `@utn_hc_fg/bg/focus` for High Contrast.
  GTK3 `@define-color` names are not CSS variables.
- **Sizes:** rail palette swatches request at least 30 x 30 px for mouse and 44 x 44 px for touch.
  Stroke tiles request at least 24 x 36 px for mouse and 44 x 44 px for touch. Palette tabs request at least
  24 px height for mouse and 44 px for touch. The footer link requests 24 px height for mouse and 44 px for touch;
  the touch switch and width slider request at least 44 px height.
- **Single colour source:** the rail dot, popover, context settings and presets are driven by
  `ToolHandler::getDisplayedColor()` and the existing colour-application path. The palette does not store an
  independent selected colour.
- **Smart Shape UI:** the context-panel minimum-size slider and the older Settings-dialog spin control were
  removed from the WIP code. The existing saved recognition setting remains supported internally for compatibility.
  This change is **not** evidence that the recognition algorithm has improved.
- **Outstanding verification:** compile/UCRT64, 96/120 DPI, large fonts, touch/stylus, High Contrast,
  dark mode, mapped target allocations, keyboard/NVDA, Blank/Freeze and Student View privacy.
