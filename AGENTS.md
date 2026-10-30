# Ultimate Teacher Notepad (UTN): agent notes

A teacher-first fork of Xournal++ 1.x. Native GTK3 only; no new UI framework. Shared by every coding agent working in this repo.

## Build and test (Windows, MSYS2 UCRT64)

```
PATH=/c/msys64/ucrt64/bin:$PATH
cd <build dir>        # out-of-tree, Debug; CMAKE_HOME_DIRECTORY must point at this checkout
ninja -j2 xournalpp test/test-gtk-integration.exe test/test-units.exe
cd test && ./test-gtk-integration.exe                          # ~2 min, 87 tests
./test-units.exe --gtest_filter='-*Soak*:*Stress*'             # 425 pass, 1 skipped (CrashChild.run)
```

A bare `ninja` builds neither test binary. Run them explicitly. Install with `cmake --install . --prefix <dir>`; run the installed app with isolated `APPDATA`, `LOCALAPPDATA` and `XDG_CONFIG_HOME`.

## UI conventions

- CSS lives in `ui/xournalpp.css`; the installed copy is what the app reads. GTK 3.24 CSS has no variables, flexbox or gap.
- Responsive decisions are in `src/core/gui/ResponsiveLayout.h` (GTK-free, unit tested). `MainWindow::updateCompactLayout` applies them.
- Mark a widget `utn-compact-hidden` to hide it below 1320 px, `utn-narrow-hidden` to hide it in the compact layout (under 1080 px).
- GtkToolbar gives wrapped widgets no overflow menu: anything that does not fit is unreachable. `MainWindow::fitAppBar` therefore measures the app bar against its window (so font, display scale and language count) and, only when it is too wide, first swaps to short wording (`utn-tight-hidden` / `utn-tight-only`), then moves buttons into a "More" menu (rank set with `utn::OVERFLOW_RANK_KEY` in `ToolMenuHandler`). Never hard-code a width or a scale: at 96 DPI the bar asks for 728 px, at 120 DPI 803 px.
- Every icon-only control needs a tooltip and an accessible name (`utn::setAccessibleName`).
- Rail tools are 38 px targets in a 60 px rail (`utn::layout::TEACHER_RAIL_WIDTH`).

## Verifying UI

- The GTK test harness has no frame clock. After a synthetic resize, widget allocations can be stale; measure with `gtk_widget_get_preferred_width` or check the real app.
- Real-app evidence: launch the installed build with an isolated profile and capture at 800, 900, 1024 and 1280 px, light and dark. Save captures under `docs/design/design-pass/after/`.
- Record what was not verified (DPI scaling, stylus, touch, High Contrast) in the pass notes rather than implying it.

## Where the context is

- `docs/design/design-pass/DESIGN-PASS-2026-10-30.md`: current teacher-rail and app-bar pass, decisions and open items.
- `docs/design/utn-design-system.md`, `docs/qa/utn-paper-ink-overnight-handoff.md`: design system and the previous handoff.

## Rules

- Commit locally, small and verified. No pushes or merges unless asked.
- Do not change annotation, classroom or Student View logic in a visual pass.
