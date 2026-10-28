# UTN paper-and-ink: overnight handoff (Claude, checkpoint 1)

Branch `feature/utn-paper-ink-implementation`, local only, based on accepted `91f40434e`.

## Done and verified (real app launched, GTK tests run)
- Pen popover was a blank bubble (card never shown): fixed in `cef5529d0`.
- Pen popover teardown on `reloadToolbars()`: popover/button `destroy` handlers null the raw pointers; one `ToolUtnContextBar` per window (no listener per reload). Test `toolbarReloadsKeepOnePenSettingsButtonAndAWorkingPopover` (5 reloads).
- Ink dock offers all five pen sizes (XS-XL), exactly one shown selected; dock lifted 64 px clear of the bottom bar; card title/tooltip names the active tool (Pen/Highlighter/Eraser/Text).
- Paper-and-ink shell CSS: segmented Teach/Mark/Present, Freeze/Blank capsule, slim icon-only rail with teal selected tile (default tool labels now Hide; a saved choice wins). Light theme seen in the real app at 1280 px.

## Tests
GTK integration 83/83 passed (heavySession/endurance cases excluded). Unit suite not rerun for these commits.

## Not done / not verified
- Rail still uses the old coloured tool glyphs and chevrons (no monochrome symbolic set yet).
- No document name / save status in the app bar; "Students" is still a text pill, not inside the capsule; menu bar unchanged.
- Properties card is still the old inspector content in a popover (colour grid, slider, preview not rebuilt); the dock-anchored popover, Pin to side, drawing, colour/width changes in the live app were not exercised by hand.
- Dark, High Contrast, 900/1024 px and Present mode screenshots not captured for these commits; the new CSS has dark/HC rules that were not looked at.
- Claude usage cannot be measured from here; no 5% monitoring is possible.

## Build
UCRT64: `PATH=/c/msys64/ucrt64/bin:$PATH; cd C:/Users/musit/Documents/utn-paper-ink-build; ninja -j2 xournalpp test/test-gtk-integration.exe`; install with `cmake --install . --prefix <dir>`; run with an isolated profile (APPDATA/LOCALAPPDATA/XDG_CONFIG_HOME) from `C:\Users\musit\utn-build-home\shots\launch.ps1`.

## Next safe actions
1. Look at dark/HC/900 px with the new CSS and fix clipping. 2. Symbolic monochrome icons for the rail. 3. Rebuild the properties card (colour grid + slider + preview). 4. Move Students into the capsule, add document name.

## Codex fallback checkpoint — `206d1459302622600fa4c0d286bbc1d06a5f2d08`

Codex continued from Claude's clean checkpoint in the same worktree. The local branch remains `feature/utn-paper-ink-implementation`; no push, merge or release occurred.

### Implementation now present
- Fresh-profile icon default uses the Lucide icon theme; explicit saved theme selections still load normally.
- Pen settings popover is rebuilt around the existing settings model: eight named colour swatches, width slider covering all five size presets XS–XL, live stroke preview, expandable advanced settings and Pin to side. Dock controls synchronize with current pen settings.
- Header displays the current document filename and clean/dirty mark from the window title. It hides at narrow compact widths. Secondary captions are reduced below 1320 px and restored above 1400 px.
- `UTN Present` adds a visual shell class only; classroom/privacy action logic was not edited.
- A focused layout regression asserts the compact rail host allocation is at most 100 px. In the standalone 900 px capture the rail is about 92 px.

### Verification (Codex self-verification)
- Debug UCRT64 build: `ninja -j2 xournalpp test/test-gtk-integration.exe` in `C:\Users\musit\Documents\utn-paper-ink-build` succeeded.
- GTK integration suite: 83 passed, 0 failed; two heavy/endurance cases excluded.
- Unit suite: 425 passed, 1 skipped (`CrashChild.run`), 0 failed.
- Focused Pen properties/dock and workspace persistence tests passed.
- `git diff --check` passed (with `core.whitespace=cr-at-eol` for this Windows checkout).
- Standalone 1280 light, 1024 light, 900 light, 1280 dark, 1280 High Contrast and 1280 Present captures are in `docs/design/visual-reset/actual-build/` and are labelled with the short source SHA. GTK integration fixture captures for popover, dock, pinned, dark and High Contrast are under `actual-build/206d1459/`.

### Remaining gaps — this is not full design acceptance
- The app is visibly improved but still does not substantially match the full concept: the rail retains mixed old glyphs and chevrons; the class controls are not a unified capsule; the menu bar remains native; and the Present status banner is absent.
- Standalone Pen popover and pin/dock interactions were not manually captured/exercised in this checkpoint; those visuals are test-fixture captures. Drawing/input pass-through near dock edges, 900/1024 dark and High Contrast, responsive keyboard focus, touch/stylus and screen-reader use remain unverified.
- Compact header omits filename at 900/1024. Present screenshot demonstrates the state-selected visual class, not all classroom state transitions or privacy copy.
- Existing custom `toolbar.ini` compatibility and all annotation/classroom workflows require further migration-stage review.
- Claude usage percentage is not measurable in this environment; fallback was taken because usage could not be measured, not because a verified 5% threshold was reached.

Next safe work is to finish visual consistency for the rail and classroom grouping, validate the contextual card and dock in the standalone app, capture the missing interaction/theme states, and run independent review. Keep all changes in this isolated branch. Do not push, merge or release.

## Follow-up after independent review — `f8b353ed73900c814e52cb60873e1d6d43fd1ca4`

Claude's read-only review of `206d1459302622600fa4c0d286bbc1d06a5f2d08` and screenshot docs commit `13ade2884a73efc7c9bd4dbcf2cfa2a2888055da` found **no blockers**. Non-blocking notes: the Lucide icon default applies only to fresh profiles; the brand and settings-caption visibility choices need deliberate follow-through; save status is inferred from the GTK title; colour tables are duplicated; width mapping depends on enum ordering; and some theme/header/Present states lack automated coverage. Student View privacy code is untouched.

Codex made one small follow-up: the compact-caption traversal now preserves the `UTN` wordmark at widths >=1280 and hides it in narrow layouts; the regression test checks that it returns in the wider hysteresis band. Commit `f8b353ed73900c814e52cb60873e1d6d43fd1ca4`.
- Build: `ninja -j2 xournalpp test/test-gtk-integration.exe` succeeded.
- Focused test `ClassroomWorkflow.appBarReducesSecondaryCaptionsBeforeItsCompactBreakpoint`: passed (1/1).
- Actual GTK captures at this commit: `ACTUAL-GTK-BUILD-f8b353ed-light-1280.png` and `ACTUAL-GTK-BUILD-f8b353ed-light-900.png`. At 900 px the wordmark is hidden and toolbar items fit. The standalone 1280 capture does not make the wordmark visually apparent despite the GTK visibility assertion, so branding in the running toolbar remains a visual follow-up rather than a confirmed success.
- The broader 83-test GTK and 426-case unit results above apply to `206d1459`, before this small follow-up; only the focused GTK test is rerun at `f8b353ed`.

The implementation and review commits remain local. Source worktree cleanliness is separate from generated screenshot artifacts; this directory still contains the test-overwritten and exploratory captures listed by `git status`.
