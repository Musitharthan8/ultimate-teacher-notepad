# UTN visual workspace implementation report

## Delivered

The UTN teacher workspace now places the tool rail and contextual inspector in a dedicated layout beside the document area. The inspector has a draggable GTK paned divider, an explicit hide button, and a rail-side restore button. A separate Workspace menu offers inspector + rail, rail only, and canvas only. The existing document-navigation sidebar remains in its own `panelMainContents` paned and is unaffected.

The inspector toolbar widget is reparented between stable containers when modes change; it is not rebuilt. Tool selection and tool settings therefore remain in their existing widgets. The canvas is likewise reparented for canvas-only mode, avoiding a blank inspector reservation. The GTK paned retains its position; the last preferred inspector width is also saved in UTN's existing custom settings element. Width is constrained dynamically, with a 190 px preferred minimum, 420 px maximum, and 520 px reserved for the worksheet when the window allows it. At narrow widths the existing control scroller allows both-axis scrolling instead of clipping settings.

The Workspace menu also offers Automatic / Show / Hide tool labels. Automatic retains the existing compact-width behavior, and icon-only buttons keep tooltips and accessible names. Resizing and workspace changes preserve the selected tool, pen colour and size exercised by the regression test. No document engine or classroom feature behavior was rewritten.

## Visual decisions

The audit of the five supplied screenshots is in [utn-visual-audit.md](utn-visual-audit.md). They showed a fixed 208 px inspector, large empty Select panels, inconsistent Pen control group alignment and button treatments, and legacy colored file/history glyphs beside newer outline tools. The GTK CSS now uses clearer neutral/raised surfaces, graphite dark surfaces, consistent border and separator states, accessible focus and touch sizes. Side inspector groups use a compact three-column grid and section labels. The UTN top-bar file/history actions use available GTK symbolic icons; the selected icon theme and project icons remain untouched. Design tokens and behavior are documented in [utn-design-system.md](utn-design-system.md).

Five actual GTK client-window captures are in [screenshots](screenshots/README.md): light, dark, high contrast, rail-only and canvas-only. They are generated from the application integration test, not mockups. Their client size is 1280 × 720.

## Changed files

- `ui/main.glade`
- `ui/xournalpp.css`
- `src/core/gui/MainWindow.cpp`
- `src/core/gui/MainWindow.h`
- `src/core/gui/toolbarMenubar/ToolMenuHandler.cpp`
- `src/core/gui/toolbarMenubar/ToolUtnContextBar.cpp`
- `src/core/gui/toolbarMenubar/ToolUtnContextBar.h`
- `test/gtk_tests/toolbar/ClassroomWorkflowTest.cpp`
- `docs/design/utn-visual-audit.md`
- `docs/design/utn-design-system.md`
- `docs/design/utn-visual-implementation-report.md`
- `docs/design/screenshots/README.md`
- `docs/design/screenshots/utn-visual-polish-light.png`
- `docs/design/screenshots/utn-visual-polish-dark.png`
- `docs/design/screenshots/utn-visual-polish-high-contrast.png`
- `docs/design/screenshots/utn-visual-polish-rail-only.png`
- `docs/design/screenshots/utn-visual-polish-canvas-only.png`

CI, packaging, release, and document-engine files were not changed.

## Verification

The focused GTK test `ClassroomWorkflow.inspectorWorkspaceModesPreserveToolStateAndPersist` verifies workspace parentage and visibility, collapsed tool switching, preservation of tool settings, persisted workspace and inspector width, divider width constraints at a 1024 px allocation, and return to the preferred width at 1280 px. Its optional capture mode creates the five PNGs above.

Final verification on the completed Stage B tree:

- `cmake --build build --target xournalpp test-units test-gtk-integration -j 4` — passed.
- `build/test/test-units.exe --gtest_brief=1` — 313/313 passed.
- `build/test/test-gtk-integration.exe --gtest_brief=1` — 35/35 passed.
- Focused capture/regression test — 1/1 passed and produced the five GTK captures.
- `git diff --check` — passed.

The GTK/Windows test environment logs known locale, unavailable tablet device, and user-profile recent-file permission warnings; these did not fail tests. The local GTK build does not provide a controllable physical monitor/DPI setup, so 1024 × 768 and Windows 150% / 200% scaling have not been visually verified. The focused GTK test exercises a 1024 px allocated paned width; the screenshots exercise the 1280 × 720 client window. A manual 1920 × 1080 and multi-scale Windows review remains appropriate.

## Commits

- Stage A workspace architecture: `a79492964` (`Add resizable UTN classroom workspace inspector`)
- Stage B visual refinement: to be filled after commit.
- Branch: `feature/utn-visual-polish`
- Branch URL: https://github.com/Musitharthan8/ultimate-teacher-notepad/tree/feature/utn-visual-polish
