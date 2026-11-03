# Phase 6 ChatGPT handoff for Claude review

**Status:** work in progress; not ready to merge. **Integration base:** `feature/utn-teacher-ux-program` at
`fe54f0749dd7cf379a1ee80fef3b2421e649208c`. **Unfinished palette base:**
`wip/phase6-palette-redesign` at `b7c272007d9a547d17dd37f580cce43cb9d4c38c`.

**Branch to review:** `wip/phase6-chatgpt-rail-followup` (read its current HEAD; do not assume this document's SHA is HEAD).
The earlier branch `wip/phase6-chatgpt-palette-accessibility` contains the rail-caption fix; a separate writer
also committed `327eeeccd832f45d83d5441774f79998cc92c8cc` on it (two accessible-label corrections).
A new branch was cut from that exact commit to avoid concurrent writers.

## Implemented (not build-verified)

- `src/core/gui/toolbarMenubar/ToolMenuHandler.cpp`: repaint the *image* within the rail button's
  icon-and-caption box rather than replacing the entire button child. Before the fix, a pen-colour change
  destroyed the visible `Colours` caption (plausible cause of `expectCaptions` failures).
- `test/gtk_tests/toolbar/ClassroomWorkflowTest.cpp`: regression test
  `changingPenColourPreservesTheMappedColoursRailCaption`, including mapped and nonzero-allocation checks;
  test `annotationPaletteTouchTargetsAreMappedAndAtLeast44Square`; test
  `smartShapeInspectorHasNoMinimumSizeSlider`. These were written before the relevant fixes where feasible,
  **but were not executed**, so red/green status is unknown.
- `ui/xournalpp.css`: guarantee touch-friendly minimum hit targets for palette link, Snap switch,
  width slider and stroke tiles. The inherited WIP already had some 44 px rules for swatches and tabs.
- Removed Smart Shape's size slider from `ToolUtnContextBar::appendShapeControls`.
  Also removed the legacy `spStrokeRecognizerMinSize` Settings-dialog spin control and its frame and
  GtkAdjustment from `ui/settings.glade`, and removed its load/save wiring from `SettingsDialog.cpp`.
  Existing `Settings::strokeRecognizerMinSize` persists internally; no settings migration was introduced.
- Updated `docs/design/UTN_DESIGN_SYSTEM.md` with the selected swatch's non-colour cue, reused colour
  tokens and palette target sizes. No new theme-engine or settings architecture.

## Tested in cloud

- GitHub read/write operations succeeded, with isolated branches and commits.
- A simple XML nesting-stack check of the updated Glade returned balanced tags and zero unclosed elements.
- Static searches found no remaining `spStrokeRecognizerMinSize` / `adjustmentStrokeRecognizerMinSize`
  / `strokeRecognizerFrrame` references in `ui/settings.glade`, no `spStrokeRecognizerMinSize` in
  `SettingsDialog.cpp`, and no `Smart Shape minimum size` slider in `ToolUtnContextBar.cpp`.
- **Not run:** CMake, Ninja, C++ unit tests, GTK tests, GTK app, Windows/WSL/MSYS2 UCRT64,
  DPI tests, stylus/touch/NVDA. Cloud container lacks GTK3 and cannot clone from github.com.
  The GitHub compare API showed focused file changes; a local `git diff --ignore-cr-at-eol` check
  was **not run**.

## Reviewed (static), critical open work

1. **Important compatibility hazard:** older profiles may still contain large
   `strokeRecognizerMinSize` values, but neither removed UI exposes the setting.
   Do not merge this phase until the fixed/adaptive recognition policy handles old values safely.
   New profiles currently use 18 page units. Do not silently erase old profile data.
2. Smart Shape geometry still uses the old recognizer. No improved least-squares circle/ellipse fit,
   Douglas-Peucker polygon corners, scale-invariant gap tolerance or robustness/handwriting tests
   have been implemented here. The existing corpus explicitly documents that ellipses are left
   as freehand, which will need coordinated changes if ellipse recognition is introduced.
3. Header capsule and bottom quick-bar defaults (task 3) have not been changed.
4. The two reported GTK regressions were **not reproduced**. A likely underlying source for
   disappearing `Colours` labels was fixed, but `interfaceModesChange... ` and
   `workspaceMenuLabelChoicesDriveTheRailCaptions` still need real GTK execution.
5. Palette and inspector callbacks must be reviewed for race/stale-tool gating, Blank-cover,
   busy, Present-hidden-tools and Student View privacy. Verify no other colour state appears.
6. The inherited palette branch was UNBUILT/UNTESTED at handoff. Do not interpret WIP code
   as delivered functionality or claim native Windows verification.

## Windows verification required

Use a separate hermetic worktree and the Nitro5 UCRT64 setup described in
`docs/design/TEACHER_UX_IMPLEMENTATION.md`. Build `all`, `test/test-units.exe` and
`test/test-gtk-integration.exe`; run unit and GTK suites at 96 and 120 DPI, with separate
test profiles. Capture the real `settings.xml` SHA-256 and modification time before/after.
Manually verify 800/1024/1280 logical widths, labelled Beginner rail, colours/presets/selection,
High Contrast/dark mode, mapped palette controls, Snap/Pin, Blank/Freeze/Present gating,
Student View privacy, stylus/touch and NVDA. Check `git diff --ignore-cr-at-eol --stat` against
`git diff --stat`; do not commit line-ending churn.

**Known limitation to retain:** Saved Classroom Themes (Marking/Corrections/Presentation/Student View Safe)
have no existing preset/workspace mapping. Only the built-in Red marking, Blue explanation and
Yellow highlighter presets exist. Do not invent a theme engine.
