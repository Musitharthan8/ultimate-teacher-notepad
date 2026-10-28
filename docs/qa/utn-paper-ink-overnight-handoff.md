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
