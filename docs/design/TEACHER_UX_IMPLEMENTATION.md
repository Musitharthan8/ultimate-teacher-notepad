# Teacher UX programme: implementation record

Handoff reference between Nitro5, the home PC, Claude and Codex. Facts only; update after each phase.

## Where things are

- Branch `feature/utn-teacher-ux-program`, worktree `C:\Users\musit\Documents\utn-teacher-ux`, build dir `C:\Users\musit\Documents\utn-teacher-ux-build` (outside the repo).
- Baseline: `aafdea359` (`design/utn-teacher-first-2026-10-30` as pushed). Its tree is identical to the verified `8b69e3e1b`.
- Phase 3 work was pushed to `origin/feature/utn-teacher-ux-program` at `216f22134`. Phases 4 and 5 were pushed later, up to `1f47c342c`; the final fix commit after it is pushed by the orchestrator. Nothing is merged.
- Also usable: `GIT_DIR=C:/Users/musit/Documents/ultimate-teacher-notepad/.git/worktrees/utn-teacher-ux GIT_WORK_TREE=C:/Users/musit/Documents/utn-teacher-ux git ...` from Git Bash (the worktree is listed "prunable" because of the MSYS path; do not prune it).
- The worktree's `.git` file uses an MSYS-style path: run git there with the MSYS git (`C:\msys64\usr\bin\git.exe`, or `. /c/Users/musit/utn-nitro5-env.sh` first). Git for Windows cannot read it.

## Build and test (Nitro5, MSYS2 UCRT64)

```
. /c/Users/musit/utn-nitro5-env.sh          # UCRT64 PATH only
cmake -G Ninja -S <worktree> -B <build> -DCMAKE_BUILD_TYPE=Debug -DENABLE_GTEST=ON -DPACKAGE_ARCH=x86_64
cmake --build <build> --parallel 2 --target all xournalpp test/test-gtk-integration.exe test/test-units.exe
test-units.exe --gtest_filter='-*Soak*:*Stress*'
test-gtk-integration.exe --gtest_filter='-*Soak*:*Stress*:*heavySession*'
```

- Run the GTK suite directly, not through the MSYS `timeout` wrapper (it mangles environment variables).
- `HermeticProfileEnvironment.cpp` points the XDG folders at a temp folder. Real profile: `%LOCALAPPDATA%\utn\settings.xml`; check its SHA-256 and mtime before and after a test run.
- DPI: `__COMPAT_LAYER=DPIUNAWARE` runs a test or the app at 96 DPI on this 125% display. GUI captures must come from a DPI-aware process, or they are crops of a larger window.

## Phase status

| Phase | Features | State |
| --- | --- | --- |
| 1 | 15 visual system, 13 document status | 13 implemented, partially verified; 15 partially implemented (see below) |
| 2 | 1, 2, 4, 10 | 4, 10, 1 implemented, partially verified (no native interaction checks); 2 already existed, audited |
| 3 | 3, 6, 8 | implemented on existing systems, automated tests and independent reviews done; native checks limited (see Phase 3) |
| 4 | 7, 11 | implemented on existing systems, automated tests and one independent review done; native checks NOT done (see Phase 4) |
| 5 | 9, 5 (+ Phase 4 stabilisation) | implemented on existing systems; full automated verification at `1f47c342c` (see Phase 5); native checks NOT done |
| 6 | 12, integration | not started |

## Phase 1

### Feature 13: document status centre

- `model/DocumentStatus.{h,cpp}`: GTK-free facts (`DocumentStatus`), mutations (`DocumentStatusModel`) and `summarise()`. Priority, most urgent first: saving, save failed, export failed, exporting, never saved, unsaved changes, recovery available, needs attention, exported, saved. A failed save clears only on a new attempt or a successful save, whatever the undo history says. Saving and exporting never change each other's state.
- Facts come from the real operations: `Control::updateWindowTitle` (undo history and path), `Control::saveImpl` (save starts), `SaveJob::afterRun` (result), `Control::exportAsPdf` + `BaseExportJob::afterRun` + `PdfExportJob::run` (export), `AutosaveJob` (failure becomes an attention item, the next good autosave removes it), `XournalMain::offerRecoveryFile` (recovery waiting).
- Presenter: `MainWindow::updateDocumentHeader` shows glyph + word, a severity class (`utn-status-ok/info/warning/error`), the full details as tooltip and the full phrase as accessible name. Only the document name hides in compact layouts; the status stays.
- The old behaviour parsed the window title. The GTK test that pinned it was rewritten to drive real state.

### Feature 15: visual system (started)

- Semantic status tokens (light, dark) and High Contrast tokens `utn_hc_fg/bg/focus` in `ui/xournalpp.css`. 82 High Contrast literals now use the tokens (same values).

## Phase 2

Verified on the final code: build exit 0; units 478 passed; GTK 100/100 at 120 DPI and at 96 DPI; real profile unchanged. Each feature's GTK tests also pass alone and shuffled, and a mutation of the key behaviour turns them red. The real app starts cleanly with it and shows the shipped rail by default.

- **Feature 4, pen presets:** `model/PenPresets.{h,cpp}` (three built-ins, custom presets, restore defaults, versioned validated saved form), `Control::{getPenPresets,updatePenPresets,applyPenPreset,activePenPresetId}`, `gui/toolbarMenubar/PenPresetsMenu.{h,cpp}` (Presets menu in the pen and highlighter settings, save/manage dialogs). A preset holds tool, colour, size and opacity (the highlighter's alpha), the properties the tools really have. A preset is marked selected only while the tool matches it. Stored in the `utn` settings element, key `penPresets`.
- **Feature 10, Beginner/Expert:** `ResponsiveLayout.h` (`InterfaceMode` and its pure rules), `MainWindow::{setInterfaceMode,getInterfaceMode}`, Workspace menu entries, `ToolUtnContextBar::appendHint` (Expert drops hints). Beginner = tool names and hints; Expert = icons only, no hints. Same tools in both. Choosing a mode sets the tool labels once; a different by-hand label choice makes the mode Custom; Teach/Mark/Present never touch it; no saved mode = Custom, so existing profiles are unchanged. Key `interfaceMode`.
- **Feature 1, customisable dock:** `model/DockLayout.{h,cpp}` (pin, unpin, hide, show, move within a group, reset; essential tools never hidden; versioned validated saved form), `Control::{getDockLayout,updateDockLayout}`, `ToolMenuHandler::{load,railItemIds,toolDisplayName}` (the teacher rail is arranged when built; the toolbar data is not rewritten), `gui/toolbarMenubar/DockCustomizeDialog.{h,cpp}` (Customise tool dock in the Workspace menu, named buttons, keyboard reachable). Key `dockLayout`.
- **Feature 2, context panel:** already implemented by `ToolUtnContextBar` for pen, highlighter, shapes, text, eraser, selection and answer boxes. Audited, nothing recreated; the Presets menu is the one addition.

### Known gaps (Phase 2)

- Fixed during native testing: the Workspace menu panel was never shown (`gtk_widget_show_all` was missing in `MainWindow::appendWorkspaceMenu`), so the Phase 2 entries were unreachable. The Presets button moved to the top of the pen and highlighter settings (applying a preset rebuilds the panel and resets its scroll), and gets a visible edge (`button.utn-presets-button`; GTK 3 puts the class on the button itself, and the toolbar rule that makes idle buttons transparent needs the same `:not()` chain to be outranked).
- Verified natively with a real mouse (120 DPI only): Workspace menu opens with every entry; Customise tool dock (pin Shapes moves it to the rail top, Reset restores it, essential tools show "(always shown)"); Presets menu opens, "Red marking pen" draws a red stroke, and the menu marks it with a bullet when reopened.
- NOT TESTED natively: saving a preset, the manage dialog, restart persistence in the real app, dark mode, High Contrast, touch and stylus. Covered by GTK tests (real widgets, real tools, settings string round trip) but not by a real mouse or keyboard run.
- The colour dot in the tool rail does not change when a preset changes the pen colour (seen in a native capture; cause not investigated); the ink-colour list in the settings panel and the drawn stroke are correct.
- Persistence is tested as the saved string round trip; loading in `Control`'s constructor is not exercised by a second `Control`.
- Expert mode does not add shortcut emphasis beyond the existing accelerator tooltips.
- Dock: only the teacher rail (`toolbarLeft1`) is customisable; the app bar and classic layouts are not. "Compact" dock presentation is covered by the Expert icon-only labels, not a separate setting. Grouping is the shipped groups plus a favourites group; custom groups are not offered.
- The dock dialog lists tools by name only (no icons).
- A key press is not bound to the dialog or the presets menu (Ctrl+K and shortcuts are Phase 4).

## Phase 3

Starting commit `3a481d92c`, branch `feature/utn-teacher-ux-program`. Work stopped early for a handover to another machine; the branch is pushed to `origin` (feature branch only, no merge, no force).

### What already existed and was reused

Teach, Mark and Present are the layouts `UTN Teacher`, `UTN Marking` and `UTN Present` (`ToolWorkspaceSwitch`, `MainWindow::toolbarSelected`; the selected layout is already saved in settings, so a restart returns to it). Spotlight and Curtain (`ToolPresentationKit`, `ToolHandler`, `PresentationOverlay`), Freeze, Blank and Student View (`ToolStudentView`), the Escape exit for overlays and the existing toggle-off-by-choosing-again were all present. Switching workspaces already kept the active tool, colour, preset, Beginner/Expert mode, zoom and document; a test now proves it.

### Added

- **Feature 6, Classroom controls:** the old "Spotlight & Curtain" popover is now "Classroom controls" (`ToolPresentationKit`, which gets Student View through `ToolMenuHandler::getStudentView`). It has a status line in words ("Classroom is normal" / "On: Spotlight, Freeze"), Freeze, Blank, Student View open/close, and one "Back to normal" that turns every effect off. `model/ClassroomMode.{h,cpp}` is the one place that names the active effects. The app-bar button carries the state in its tooltip and accessible description.
- **Escape:** `MainWindow::escapeClassroom` also takes down the Blank cover that hides the teacher's own canvas. It leaves Freeze and a projector-only Blank alone, and does nothing while a popover or menu holds the pointer grab, so Escape keeps closing popovers and cancelling text edits.
- **Defect fixed:** turning Blank off did not take the teacher cover down when Student View had been opened while the cover was up, leaving the teacher behind a cover that hid the only controls. Found by the Escape test; fixed in `ToolStudentView::setBlanked`.
- **Feature 3 and 8, workspaces and Present:** `MainWindow::leavePresent` returns to the workspace Present was entered from (Teach if that layout is gone); Present hides the tool dock until revealed (`setPresentToolsRevealed`; a later Present starts quiet again); Ctrl+Shift+1, 2, 3 choose Teach, Mark, Present, Ctrl+Shift+P enters or leaves Present, Ctrl+Shift+T shows or hides the dock in Present (`MainWindow::workspaceShortcut`; skipped while an entry or text view has focus, reads the key without Shift so other layouts work). In Present the Classroom controls list "Show tools" / "Hide tools" and "Leave Present".

### Verification

- New tests: `ClassroomModeTest` (5 unit tests) and in `ClassroomWorkflowTest`: classroom controls and Back to normal, Escape, workspaces keep tool/dock/mode/page and Present returns, Present hides and reveals the dock, shortcuts, Present's visible reveal and leave controls. Mutation checks turned red for: Escape's cover branch, `leavePresent` always going to Teach, Present not hiding the dock, and the Blank-cover fix (the Escape test was red before the fix).
- Phase-close suite results are recorded in the final report of the session (units, GTK at 120 and 96 DPI, real settings file hash before and after).
- Independent reviews (read-only agents): C++ reviewer, accessibility architect, security reviewer (privacy of classroom transitions), silent-failure hunter. No critical finding; the privacy review found every transition path safe (restore, unblank and unfreeze re-render through the layer filter before painting). Functional findings fixed: `leavePresent` with a deleted layout, stale `workspaceBeforePresent`, shortcuts in text entries and on non-US layouts, Escape taking a popover's Escape, the reveal button's initial label.
- Native checks: the Classroom controls popover rendered correctly at 1280 and its entries match the design. A native Blank test opened Student View across the display while the user was screen sharing in a video call; native checks were stopped for that reason. Native keyboard, Escape, Present and shortcut checks are NOT TESTED.

### Known gaps (Phase 3)

- Native (real mouse and keyboard) checks of Escape, Present, the shortcuts and the leave and reveal controls are NOT TESTED. Narrow widths (800, 1024) and the narrow-height case were not re-captured.
- Accessibility findings not fixed: no announcement to a screen reader when the classroom status changes after the popover closes; Freeze, Blank, Spotlight and Curtain are relabelled buttons rather than toggle buttons with a pressed state; the Blank cover is not an alert role and the controls behind it stay in the Tab order; focus is not moved before the Present dock hides; the shortcuts are not GAction accelerators, so they are neither listed in menus nor rebindable.
- Present has no auto-reveal when the pointer reaches an edge, only the button and the shortcut.
- Hardening items from review, not done: `ToolStudentView::updateState` listeners are moved aside during notification (fine while listeners only read state); the tool-changed listener added per popover build is never removed (small growth per toolbar rebuild); `restoreNormal` notifies listeners twice; closing Student View while Blank or Frozen clears both; the popover's Freeze is greyed out without Student View and does not explain why.
- Cross-feature scenarios 5, 6, 7 and 8 are covered in part by the automated tests; scenarios 9 and 10 (unsaved annotations through Present, restart after classroom use) have no automated test.
- Not started: `ecc:i18n-sync` for the new strings, `awesome-privacy-audit` and a full skill inventory matrix. Skills actually invoked this phase: `caveman:investigate-first`; `ecc:team-builder` selection led to the four agent reviews above.

## Phase 4

Start `216f22134` (Phase 3 plus the project report; `09321b85b` is its parent code commit; pushed, verified against `origin`). End `e6017b5c5`. Local commits, oldest first: `93f2fcb29` pure rules, `97fcebbbc` Lesson Navigator, `d0514a301` Command Palette, `3dc75d0a9` menu entry + 300-page test + order-proof tests, `e6017b5c5` review fix. Nothing pushed, merged or rewritten.

### What already existed and was reused (Feature 7)

`ToolLessonNavigator` (the "Lesson Navigator" popover: Page n/N, previous/next, per-page label with undo, quick labels, lesson map, previous/next labelled), `XojPage::utnPageLabel` (saved per page for teacher exports only, never in student exports, followed automatically by insert/delete/move), `NavigationHistory` (holds `PageRef`s; Alt+Left/Right), the sidebar page thumbnails (`SidebarPreviewPages`, drawn in the widget `draw` handler so only painted entries render), the PDF outline tab, `GOTO_PAGE`. **No new page manager and no new stored field.**

### Added: Feature 7, Page Navigator

- `model/PageNavigation.{h,cpp}`: typed page number rules, bookmark toggle, adjacent marked page, recent pages (GTK-free, 9 unit tests).
- Lesson Navigator popover now has: a "Go to page number" box with an error line ("Enter a page from 1 to N"), "Bookmark this page" / "Remove bookmark" / "Page is labelled", "Show page thumbnails", the current page marked in the lesson map (bold, edge, accessible name "..., current page"), and "Recent pages" (up to 5, from `NavigationHistory::visitedPages()`, resolved through `Document::indexOf`, so inserted, deleted and moved pages are followed). The panel scrolls in short windows.
- A bookmark is the page label `Bookmark` (undoable, saved with the page). A page with a label of its own is never overwritten or cleared by the button (the button reads "Page is labelled" and is greyed).
- The operations are static on `ToolLessonNavigator` (`goToPage`, `toggleBookmark`, `showThumbnails`) so the Command Palette runs the same code.

### Added: Feature 11, Command Palette

- `model/CommandPalette.{h,cpp}` (GTK-free: ranking, accessible label, page-number query; 8 unit tests) and `gui/CommandPalette.{h,cpp}`.
- Ctrl+K handled in the existing main-window key hook in `MainWindow` (no other Ctrl+K binding existed; only exactly Ctrl, so plain `k` and Ctrl+Shift+K are untouched). Also "Command palette (Ctrl+K)..." in the Workspace menu. Ctrl+K again closes it.
- Modal window with a search box, a list, Up/Down (skip unavailable rows), Enter, Escape. Unavailable commands are listed in words ("Unavailable: reason"), cannot be selected and cannot run; accessible name carries category, shortcut and availability. Teacher commands rank first (tools 90, pages 80, classroom 75, workspaces 70). A typed page number offers "Go to page N".
- Commands run the existing code: tools = `Action::SELECT_TOOL`; pages = the `GOTO_*`, `NEW_PAGE_*`, `DELETE_PAGE` etc. actions; workspaces = `MainWindow::toolbarSelected/leavePresent/setPresentToolsRevealed` (same as Ctrl+Shift+1,2,3,P,T); classroom = `ToolPresentationKit::runClassroomAction`, new `ToolPresentationKit::backToNormal` (the popover button's own function) and the same `ToolStudentView` methods the buttons call. Freeze is unavailable until Student View is open, exactly like the popover.
- Focus returns to the widget that held it. Page labels are never listed or searchable (they may be on screen while the class watches).

### Verification (as of Phase 4 close, code `e6017b5c5`; later numbers in Phase 5)

- Build exit 0 (`-j2`). Units 500 passed, 1 skipped (was 483). GTK 119/119 at 120 DPI and 119/119 at 96 DPI (was 109). Real `%LOCALAPPDATA%\utn\settings.xml` SHA-256 `6c73cda17090...cdb44` and mtime `2026-10-30 11:40:33` unchanged before and after.
- Phase 3 verification closed by automated tests only: `ClassroomWorkflow` (Escape, Present enter/exit, Ctrl+Shift+1/2/3/P/T, dock reveal, workspace restore, Student View privacy, no shortcuts while typing) passed 80/80 at the start of Phase 4 on the unchanged code.
- New GTK tests (all in `ClassroomWorkflowTest.cpp`): typed page + tool/zoom kept + no undo entry; bookmark + undo/redo + teacher label protected; recent pages through insert, move, delete + current page marked; save/reopen keeps the bookmark; thumbnails open the sidebar tab; 300-page rapid navigation (50 jumps, slowest 155 ms incl. a 60 ms settle); Ctrl+K open/Up/Down/Enter/Escape/focus return; unavailable commands; workspace + classroom commands; typed page, bookmark, labels never listed; Enter before the search entry reports (found by review, red before fix).
- Mutation checks turned tests red for: Ctrl+K modifier check loosened; focus restore removed; Enter-before-search (before the fix).
- Independent review: `ecc:cpp-reviewer` (read-only). 4 findings, none critical: Enter on a stale list (fixed, tested), focus restore during window destruction (fixed), `describeCommands` under an open list (fixed), raw `MainWindow*` in the deferred call of the Workspace menu button (fixed in Phase 5 Task 1: the call now holds a weak `alive` token). No privacy finding.
- Skills/agents actually used: `superpowers:using-git-worktrees` (worked in place; the branch is already a linked worktree), `superpowers:test-driven-development` (models and navigator: red first; palette window was written before its GTK tests, then tests added and mutation-checked, a departure from strict TDD), `ecc:cpp-reviewer`. NOT used, although listed in the brief: `ecc:dev-team`, `product-lens`, `codebase-onboarding`, `iterative-retrieval`, `inherit-legacy-style`, `contract-first`, `design-system`, `frontend-a11y`, `click-path-audit`, `windows-desktop-e2e`, `i18n-sync`, the awesome-* and remaining caveman/superpowers skills, accessibility and security agents. Codebase reading was done directly.

### Native GUI verification: NOT DONE

No real mouse/keyboard run of the navigator or the palette (Ctrl+K on a real keyboard, popover look, palette window position and size at 800/1024/1280, dark mode, High Contrast, touch). Fullscreen Blank / Student View were deliberately not activated (shared desktop; GTK still reports two monitors on this machine). The GTK tests drive real widgets and key events through `gtk_main_do_event`, not a human.

### Performance and privacy findings

- 50 jumps over a 300-page document with the sidebar open: slowest 155 ms including the test's 60 ms settle; no thumbnail is rendered by the navigator itself (existing draw-on-paint). Not measured on a real large PDF.
- Navigation adds no undo entry and does not change content. Labels are teacher-only data: not in student exports (existing), not in the palette. The Lesson Navigator popover still shows labels, as before.
- Test hygiene found: earlier GTK tests can leave a modal `GtkMessageDialog` and a disabled `SELECT_TOOL`; the palette tests clear both first. Not investigated which test leaves them.

### Known gaps (Phase 4)

- Bookmark is a label named "Bookmark": two bookmarks look identical in the map except by page number; no rename-on-bookmark. A document opened from a student export has no labels at all.
- Recent pages hold at most 5 and come from the 50-entry history (also fed by search jumps); entries are not persisted across sessions.
- Page thumbnails are not inside the popover (the button opens the existing sidebar tab). PDF outline sections are in the existing outline tab, not in the popover.
- Palette: no fuzzy typo matching; commands are English keywords with translated titles; no recent-commands memory; shortcut labels read from GTK accelerators ("Ctrl+Shift+..." for workspaces is fixed text); no pen-preset or dock commands yet; "Delete page" is offered (undoable, same as the menu).
- Palette and navigator strings are not in the translation files (`i18n-sync` not run).
- Accessibility not audited by an agent: the palette status line is a plain label (no live-region announcement of the result count), the palette is undecorated (accessible name set), row Up/Down is custom key handling over a `GtkListBox`.

### Recommendations for Phase 5

Run the native pass first (real keyboard: Ctrl+K, Go to page, bookmark, thumbnails; two window widths; dark and High Contrast) on a non-shared desktop, then the a11y agent on the palette and navigator. Then Features 9 and 5 on the existing systems, and fold the palette into any new feature by adding one `addAction` line.

### Known gaps (Phase 1)

- Verified (final code): build exit 0; units 446 passed; GTK 94/94 at 120 DPI and at 96 DPI; real profile SHA-256 and mtime unchanged. Native captures at 1280 and 800 (120 DPI) show the status; dark mode, High Contrast, 96 DPI captures and keyboard checks of the status are NOT TESTED.
- Not covered by an automated test: the new-document reset through `Control::replaceDocument`, and the recovery wiring in `XournalMain` (needs the dialog flow). The model rules for both are unit tested.
- `beginSave` / `beginExport` stay "running" if the job's completion never fires (no window, scheduler stopped).
- The autosave attention item is matched by its translated text; a locale change mid-session would not match.
- A brand-new empty document shows an amber "Not saved yet"; consider a calmer severity while there is nothing to lose.
- Export reporting covers "Export as PDF" only. `CustomExportJob` (image export, export dialog) does not report its result yet.
- No actionable popover on the status (Save now / Retry); the tooltip carries the details.
- Visual system: remaining non-High-Contrast literals and a written type/spacing/size scale are not done.

## Phase 5

Start `308e6bd18`. Commits, oldest first: `d4d850818` + `d876e07ce` Task 1, `e743280f4` + `82e42c8e4` Task 2, `e10354ef6` + `d7e9f9b9f` + `146a4673d` Feature 9, `22b7d0b60` + `1f47c342c` Feature 5, then the final verification fix commit.

### Verification at `1f47c342c` (before the final fix commit)

- Units 527 passed, 1 skipped (`CrashChild.run`, the crash-handler child helper, skips by design), 0 failed (528 tests).
- GTK 155/155 at the default 120 DPI and 155/155 with `__COMPAT_LAYER=DPIUNAWARE` (96 DPI). The tests print no DPI, so the shim's effect is not proven by the tests themselves. Excluded: `longSession*`, `heavySession*` (no test is named soak/stress).
- New tests of Phase 5, each run alone: 63/64. Shuffled (seeds 777 and 31337): GTK 36/37 each, units 27/27 each. The two order problems found are fixed in the final commit (below).
- Real `%LOCALAPPDATA%\utn\settings.xml` unchanged (SHA-256 `6c73cda17090...cdb44`, mtime `2026-10-30 11:40:33`). The GTK tests write their settings into the throw-away profile of `HermeticProfileEnvironment.cpp`: `Util::getConfigFile("settings.xml")` resolves to `%TEMP%\utn-gtk-test-XXXXXX\XDG_CONFIG_HOME\utn\settings.xml` in the test process (checked with a one-off probe).

### Task 1: Phase 4 stabilisation

- `Control::block/unblock` count a depth (`blockDepth`), so nested jobs keep the application busy until the last one lets go; `Control::isBlocked()`.
- Command palette: never opens over the Blank cover and closes when the cover goes up (the palette is its own window, which the cover does not hide); refuses every command while busy (reason "Busy saving or exporting"); the Workspace menu's deferred call holds a weak `alive` token; Lesson Navigator labels follow bookmarks from the palette and their undo/redo.
- Test hygiene: the fixture closes left-over message dialogs, unblocks, clears undo state and lowers the cover between tests.

### Task 2: accessibility and usability

- Focus after a workspace switch, the Present dock hiding or leaving Present lands on a visible control (`MainWindow::keepFocusUseful`).
- Unavailable palette rows can be reached with Up/Down and say why (status line and ATK description); Enter on one runs nothing.
- Blank cover focus trap: while the cover is up, its Show page button is the only Tab stop (the content behind it is insensitive); on lowering, sensitivity and the earlier keyboard focus come back (the page when that widget is gone).
- Lesson map and Recent list follow a page clicked inside the open popover; Freeze/Blank buttons keep names that contain their visible label.

### Feature 9: floating selection toolbar

- `gui/SelectionToolbar`, `model/SelectionToolbarPlacement` (7 unit tests). Delete, Duplicate, Colour, Bring to front, Send to back beside the current selection, running the same Control functions as the properties panel. A child of the main overlay, never a window of its own. Shown only in teacher layouts (hidden in UTN Present until its tools are revealed), with a selection tool, the setting on (Workspace menu, key `selectionToolbar`), not busy, Blank cover down, and the selection on the current page and on screen. Escape puts it away for that selection; it never takes the keyboard when it appears.
- Duplicate copies into the source layer (never the clipboard). Undo: `duplicateSelection` first finalises the selection, which can push its own Move step when the selection had been moved or resized, so up to two Undos bring the page back; the copy itself is one `InsertsUndoAction`.
- Known gaps: placement does not avoid the handles beyond a 14 px gap; on a canvas narrower than the row the buttons stack in a column (final commit); no shortcut to jump into it; strings not in the translation files; colour-hidden case tested with a stroke selection as for a picture, not a real image.

### Feature 5: Annotation Bank

- `model/AnnotationBank.{h,cpp}` (GTK-free, 17 unit tests): built-in entries tick, cross, star (symbols, written at twice the Feedback size, at least 24 pt), "See me", "Well done" (comments); comments of the teacher's own; restore defaults; versioned saved form `v1;style|rrggbb|label|text;...` (escaped), at most 24 entries, label 40 bytes, text 120 bytes; entries with an unknown style, a bad colour, blank or over-long fields, control characters, invalid UTF-8 or a repeated text are dropped; anything unreadable gives the defaults; an emptied bank stays empty. `placeAnnotation` centres in the visible part of the page and keeps it on the page.
- Stored in the `utn` settings element, key `annotationBank` (absent = defaults). `Control::{getAnnotationBank,updateAnnotationBank,annotationUnavailableReason,insertAnnotation}`.
- Insert: an ordinary `Text` element (plain Feedback typography, entry colour). `LayerController::ensureVisibleInkLayer(false)` first (also when hidden answers show faintly), refused if the layer is still hidden or hidden answers; `Layer::addElement` under the document lock + one `InsertUndoAction`; then `utn::notifyStudentVisibilityChanged(true)`. When the page has only hidden answers, `ensureVisibleInkLayer` makes a new layer; its `InsertLayerUndoAction` is taken back out (`UndoRedoHandler::takeLastUndoAction`) and grouped with the insert in one `GroupUndoAction` (insert first, so Undo empties the layer before removing it), so one Undo removes both. Refused while `Control::isBlocked()`, while the teacher Blank cover is up, in UTN Present while its tools are put away, and on a page without layers (`annotationUnavailableReason`).
- UI: an "Annotation Bank" page in the existing Feedback popover (`gui/toolbarMenubar/AnnotationBankPanel.{h,cpp}`, in `ToolTeacherStamp`; the notebook tabs scroll), so it is in Teach, Teach Tablet and Mark and absent from Present. One button per entry (accessible name = its visible label, tooltip "<label>: <what it does>"), a "Remove" button per entry (name "Remove <label>", set apart; removal is not undoable and is reported as "Removed: <label>"), a comment box (name starts with its placeholder) with "Add to bank" (Enter adds; "shortened to fit" when cut to 120 bytes) and "Restore defaults" (asks in place: "Confirm restore", then "Defaults restored."). The status line has the ATK status-bar role and is re-set on every message. Rebuilt when the popover opens. While unavailable, the entry buttons stay focusable with the reason as accessible description, tooltip and status line; activating one only says why. The keyboard stays in the panel after Remove (next row's Remove, else the one before, else the comment box) and after a refused insert (the same entry). The popover closes after an insert. Star colour `#9A5B00` (about 5:1 on white).
- Unsupported: picture or sticker stamps (no image entries: they would need a new stored asset), per-entry size or font, reordering entries, editing an entry in place (remove and add again), choosing a colour for a new comment (it takes the Feedback colour), placing at the pointer (the existing Feedback comments still place with a tap). No new document format: the page holds plain text elements.
- Not done: Annotation Bank entries in the command palette, translation files, native check (see below).

### Final verification fixes

- Blank cover: forward Tab could leave the cover for the hidden PDF floating toolbox (a sibling of `mainBox` in the main overlay, still mapped inside its revealer); the other overlay children are now insensitive while the cover is up and come back as they were. The test failed when run alone and passed in the full suite only by the order.
- The command palette stays shut in UTN Present while its tools are put away, and closes when Present is entered or its tools are hidden again.
- Selection toolbar: a column instead of a row when the canvas is narrower than the row; a dismissal is forgotten whenever a selection goes; the setting is saved at once.
- No ATK description is set to NULL any more (`ToolStudentView`, `ToolPresentationKit`); the rail scroller's maximum width is lifted before its minimum is raised; the busy cursor is not set on a window that is not realised yet.

### Native checks for a person (not done by the automated tests)

- Blank cover: Tab and Shift+Tab stay on Show page; focus comes back to where it was after Show page.
- Screen reader (NVDA): greyed palette rows read their reason; Freeze/Blank names and states; the Annotation Bank status line is announced.
- Switch workspace (Ctrl+Shift+1/2/3, Present and back) and press Tab: the keyboard is on a visible control.
- Lesson Navigator popover: clicking a page moves the marker and keeps the keyboard in the popover.
- Selection toolbar: placement above/below the selection, scrolled and zoomed, with the scrollbar on the left, in a narrow window (column), over the ink dock and the Spotlight banner.
- Annotation Bank: Tab order inside the Feedback popover page; the glyphs ✓ ✗ ★ in the Windows fonts; an insert lands in the middle of what is on screen; popover width with six tabs; Student View shows the mark at once (single display, not fullscreen); Confirm restore; dark mode and High Contrast looks.
