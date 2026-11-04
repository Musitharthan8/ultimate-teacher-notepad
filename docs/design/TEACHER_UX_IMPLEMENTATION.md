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
| 6 | 12 (+ integration) | 12 implemented on the existing undo history, targeted automated tests only; native checks NOT done (see Phase 6) |

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
- UI: an "Annotation Bank" page in the existing Feedback popover (`gui/toolbarMenubar/AnnotationBankPanel.{h,cpp}`, in `ToolTeacherStamp`; the notebook tabs scroll), so it is in Teach, Teach Tablet and Mark and absent from Present. One button per entry (accessible name = its visible label, tooltip "<label>: <what it does>"), a "Remove" button per entry (name "Remove <label>", set apart; removal is not undoable and is reported as "Removed: <label>"), a comment box (name starts with its placeholder) with "Add to bank" (Enter adds; "shortened to fit" when cut to 120 bytes) and "Restore defaults" (asks in place: "Confirm restore", then "Defaults restored."). The status line has the ATK status-bar role and is re-set on every message. Rebuilt when the popover opens. While unavailable, the entry buttons stay focusable with the reason as accessible description, tooltip and status line; activating one only says why. The keyboard stays in the panel after Remove (next row's Remove, else the one before, else the comment box) and after a refused insert (the same entry). The popover closes after an insert. Star colour `#9A5B00` (about 5:1 on white). Phase 6 polish (static, not built or run on this machine): messages wrap at about 32 characters and the entry list keeps a 260-380 px width, so a long message or entry never widens or clips the popover; "Confirm restore" gets the danger style and is forgotten when focus leaves the button (a later click cannot replace the bank by surprise); unavailable entries get a dashed border (`utn-bank-unavailable`) besides the dimmed label, so they do not look ready; the comment box has an explicit focus ring in light, dark and High Contrast.
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

## Phase 6

Start `7dc87932a`.

### Feature 12: Undo History panel

- Reuses the existing history: `UndoRedoHandler` (its two lists, `UndoAction::getText`, the `UndoRedoListener` callbacks). No second undo system, nothing stored. Small additions to the handler: `undoCount/redoCount`, `undoTexts/redoTexts(max)` (newest first), `lastStepSucceeded()` (the result `undo()`/`redo()` already computed), `removeUndoRedoListener` (safe while listeners are being told: the slot is blanked and dropped afterwards). `Control::closeDocument` now tells every listener, not only Control itself (found by the shuffled tests: the panel kept listing a closed document's steps).
- `model/UndoHistory.{h,cpp}` (GTK-free, 8 unit tests): newest first, at most 50 steps (half each side of the current point; a short side leaves its room to the other), hidden counts, "Change" for a step without words (also GroupUndoAction's "!! NOTHING !!"), row texts ("Write text (next Undo)", "Change color (next Redo, undone)", "Delete"), accessible names that start with the visible text and say what choosing the row does, and the gate: covered (Blank) and quiet Present hide the list, busy only refuses (privacy wins over busy).
- UI: a "History" page in the Classroom popover of the tool dock (`gui/toolbarMenubar/UndoHistoryPanel.{h,cpp}`, in `ToolClassroomWorkflow`, whose tabs now scroll), so it is in Teach, Teach Tablet and Mark and absent from Present; and "Undo history" in the command palette (Edit), which opens that page with the keyboard on what Undo would undo. Status line (ATK status bar, set anew each time) saying what Undo takes away and Redo brings back; Undo and Redo buttons labelled as the menu items are ("Undo: Write text"), still focusable when they cannot run, with the reason as description, tooltip and status; a hint line; the list with a "You are here" row between undone (above, dimmed) and done steps; a line saying how many steps are not listed. Rows are cut short at 40 characters with the whole text in the tooltip; the page asks for 323 px and the popover stays at 402 px (same as without the page). Up/Down, Home/End and Page Up/Down move between rows (handled by the panel: GtkListBox's own keys start from a cursor that a row given the keyboard by the panel does not get), Enter or Space chooses one. The list follows every change through its listener (coalesced to one idle), and the keyboard stays at the same place in the rebuilt list.
- Privacy: never in the Student View frame (the panel is only in the teacher window's Classroom popover). Under the Blank cover and in Present with its tools put away the list is emptied and hidden, the popover closes (the cover makes the content insensitive, which the panel watches), the palette entry is unavailable with the reason, and a jump in progress stops. Step descriptions are the action names ("Write text"), never element contents.

### Jump to a step: implemented, and why it is safe

Choosing a done step undoes back to before it; choosing an undone step redoes up to and including it. It is nothing but the ordinary action, `Action::UNDO`/`Action::REDO` fired through the ActionDatabase (the same GAction as Ctrl+Z/Ctrl+Y, so a disabled action does not run and `UndoRedoController` clears and restores the selection as it does for a key press), one step per main loop turn (idle), so redraws and the window title run between steps as between key presses. Before each step: the gate must be open (not busy, no Blank cover, not quiet Present), the action enabled, and the history unchanged since the previous step left it: the undo and redo counts and the handler's history revision (a counter bumped by every add, undo, redo, take-back and clear) must all match, because equal counts alone cannot tell one step replaced by another. After each step: the redo and undo counts must have changed by one step in the right direction, the revision must have gone up by one, and `lastStepSucceeded()` must be true. Anything else stops the jump where it is and says so ("Stopped after 1 of 3 steps: a step could not be completed"). The saved and autosaved points are the handler's own pointers to step objects, so going back to the saved step shows the document as unchanged (tested for the saved point in the GTK test, and for both points in the unit tests); a jump neither saves nor autosaves. A failed step behaves as with Ctrl+Z: the handler shows its own error and the step moves to Redo; the jump does not go past it. At most 50 steps (the listed ones). The one-step Undo and Redo buttons make the same check: they say "Undid"/"Redid" only when one step really moved across without failing, else "Could not undo" or "Undo did not complete: the history changed". A save or export that starts or ends refreshes an open panel at once (`UndoRedoListener::undoRedoBusyChanged`). Listeners are told only if they were registered when the telling began, and registering twice is ignored.

### Tests

- Units: `UndoHistory.*` 8/8 (red first: they crashed on the empty stub), also shuffled with seed 777.
- GTK (`ClassroomWorkflow.undoHistory*`, 6 tests, each alone and shuffled with seed 777, 6/6 both): real steps (Annotation Bank tick, selection toolbar Colour and Delete, new page) listed newest first with the marker, label-in-name, tooltips, Undo/Redo buttons, Redo cleared by a new step, "Nothing to redo" reachable; the Annotation Bank grouped layer + mark is one row and one Undo; a jump of three (including a selection move) is three ordinary Undo notifications, keeps the saved point, keeps the keyboard on the chosen step, and redoes back; jumps stop at a failing step, at a history changed between steps, and while busy (list still readable, buttons say why); Blank cover closes the open popover and empties the list, the palette entry is refused, Present has no Classroom popover; palette opens the page, 50-step bound with "10 earlier steps are not listed.", long text ellipsized with the full tooltip, keys, focus kept across a rebuild and a listener refresh, popover no wider than before. Related filter `ClassroomWorkflow.*alette*`: 13/13 (the palette catalog test now knows `edit.history`).
- The main window's popovers are not mapped on this test display (the Lesson Navigator tests skip for the same reason), so the keyboard and focus checks run on the same panel hosted in a window of its own; the "keyboard on the next Undo after the palette opens it" check only runs where the popover maps (recorded as `history_popover_mapped`).
- Departure from strict TDD: the panel was written before its GTK tests; the first runs were red for real faults (popover 700 px wide, keys not moving, stale list after a cleared history) and were fixed in code.
- Not run: the full unit and GTK suites, the app build target, 96 DPI.

### Known gaps (Feature 12)

- Only the listed 50 steps can be jumped to; older steps need repeated jumps.
- Step names are Xournal++'s own `getText()` strings (some in English only, e.g. "Move selection to layer"); a group shows its first step's name. New strings are not in the translation files.
- No confirmation before a long jump (it is undone by choosing the step again from the other side). A jump of many page steps runs as fast as the main loop allows.
- Not reachable in Present even with its tools revealed (Present's dock has no Classroom button); the palette says so.
- The palette's reason is computed when the palette opens.

### Native checks for a person

- Classroom popover: History tab position and look in Teach and Mark, light, dark and High Contrast; six tabs scroll at 800 and 1024 wide.
- Ctrl+K, "Undo history", Enter: the popover opens on History with the keyboard on the next Undo row; Up/Down/Home/End/Page keys, Enter on a row, Tab to the Undo and Redo buttons, Escape closes.
- Draw a few strokes, add an Annotation Bank tick, move and recolour a selection, add a page: the list follows at once; jump back three steps and forward again; the title's unsaved marker matches.
- Open the History, then raise Blank (single display, not fullscreen): the popover closes and stays closed.
- NVDA: row names, the status line announcement after a jump, the greyed Redo reason.


## Phase 6: ChatGPT cloud handoff (2026-11-03, review required)

**Never treat this section as a verified Windows build.** All commits below were created through the
GitHub connector and reviewed statically. No CMake/Ninja, C++ unit suite, GTK integration suite,
Windows UCRT64 test, native screen capture or real-profile hash check ran. The affected
`wip/` branches have **not** been merged into `feature/utn-teacher-ux-program`.

The source ancestry is
`fe54f0749` → original unfinished `wip/phase6-palette-redesign` (`b7c2720`) →
`wip/phase6-chatgpt-palette-accessibility` →
`wip/phase6-chatgpt-smart-shape` →
`wip/phase6-chatgpt-classroom-capsule` →
`wip/phase6-chatgpt-design-system`.
Review the **head of the last branch** for the cumulative changes. Never force-push or
rewrite the integration branch. GitHub comparison was used to confirm ancestry and
changed-file scope; line-ending equivalence still needs checking in the Windows worktree.

### What to review before cherry-picking

1. **Palette**: carries forward the incomplete concept UI from `b7c2720`. Follow-up work
   fixes accessible labels, routes Tool Profiles through `ToolHandler::setDisplayedColor`,
   preserves translucent highlighter opacity, increases palette touch targets to 44 px,
   and adds the selected swatch High Contrast cue. The `Colours` rail indicator now
   preserves its label-bearing GTK box instead of replacing the entire child.
   `test/gtk_tests/toolbar/ClassroomWorkflowTest.cpp` includes a mapped-and-allocated
   caption regression assertion. Check visual popover mapping at 800/1024/1280 and
   both DPIs, and the existing `interfaceModesChange...` and `workspaceMenuLabelChoicesDriveTheRailCaptions` tests.
2. **Smart Shape**: the only found `Smallest shape` UI slider was removed from
   `ToolUtnContextBar.cpp`. The recogniser now ignores a legacy saved
   `strokeRecognizerMinSize` and applies an 8 page-unit minimum. It compares
   endpoint gaps to drawn perimeter, attempts a conservative three/four-corner
   Douglas–Peucker fallback, and then attempts circle/ellipse least-squares fitting
   after the original recogniser. Review algebra, pathological small strokes,
   result accuracy and line/handwriting false positives. New GTK-free GTests are in
   `test/unit_tests/control/ShapeRecognizerRobustnessTest.cpp`. **These tests have
   not been compiled or executed; do not assume they pass.**
3. **Classroom**: new profiles enable the existing bottom ink dock by default;
   an explicit saved value wins. The app-bar presentation label is
   `Spotlight & Curtain`; the existing Students, Freeze, Blank and presentation
   actions remain distinct widgets with a consistent rounded appearance.
   High Contrast and touch CSS have additional rules. Ink dock callbacks and
   visibility re-check `ToolUtnContextBar::toolControlsReachable`, with a new GTK
   regression scenario for Blank and hidden Present. The test reacquires toolbar
   controls after workspace changes to avoid stale pointers.
4. **Documentation**: `UTN_DESIGN_SYSTEM.md` now records the real eight ink/four
   highlighter choices, selection tick + ring, GTK3 colour tokens, 44 px touch
   targets, appearance defaults, gate, and limitations. Verify the stated
   CSS minimums against mapped GTK allocations.

### Required verification before merging

From an isolated MSYS2 UCRT64 worktree, check line endings
(`git diff --ignore-cr-at-eol --stat` versus `git diff --stat`), configure
Debug with tests, build `all`, run unit tests and GTK tests at 96 and 120 DPI,
and check the real `%LOCALAPPDATA%\\utn\\settings.xml` SHA-256 and mtime remain unchanged.
Validate Student View audience filtering, Blank/Freeze, Present hidden tools,
High Contrast, dark title bar, stylus, touch, keyboard, and NVDA.
Test each new recogniser case and inspect representative drawn outlines;
reject the changes if they create false-positive shapes from handwriting.
Update this handoff with **actual** exit codes and test counts before declaring a phase complete.

**Known product limitation remains**: Saved Classroom Themes (Marking, Corrections,
Presentation, Student View Safe) are not implemented or mapped to existing workspaces.
Only the existing Red marking, Blue explanation and Yellow highlighter pen presets exist.
No theme engine, settings redesign or document-format migration was added here.


### Phase 6 follow-up: static hardening

The isolated `wip/phase6-chatgpt-hardening` branch continues from `wip/phase6-chatgpt-design-system` (not merged). Changes:
- Polygon closure now permits a modest 18% gap relative to drawn length; circle closure remains at 14% to avoid converting open C-like handwriting into a loop. The *legacy* inertia-circle path now also checks closure, which previously only the new least-squares path checked.
- Synthetic tests cover a scaled small gapped triangle (0.5x/1x/4x), open-C negative case, and fitted circle/ellipse bounds. The Douglas–Peucker fallback declines contours longer than 4096 points, avoiding pathological recursive work during dense stylus input.
- `Control::applyDisplayedColour` now gives an open TextEditor the **effective** `ToolHandler::getDisplayedColor()` after a swatch change rather than the raw swatch input. This preserves Feedback/Answer Box alpha. A GTK regression case edits an Answer Box with alpha `0x70`.
- Independent Python geometry sanity check, *not a C++ project test*: for the current synthetic fixtures, endpoint gap / drawn length was 6.16% (small gapped circle), 3.43% (rotated ellipse), 16.89% (gapped triangle), 17.09% (its 0.5x/1x/4x variants), and 20.13% (open C). These satisfy the intended 18% polygon/14% circle gates. No claim that any recogniser GTest has passed.
- Required before integration: run the Windows UCRT64 build and full hermetic unit/GTK tests, verify geometry output and handwriting negatives, and inspect legacy-call compatibility with the stricter circle closure. Verify the live Answer Box text colour/alpha test and ensure no regression in existing TextEditor interactions.


### Phase 6 follow-up: legacy circle non-finite guard (2026-11-04)

On isolated branch `wip/phase6-chatgpt-loop-finite-guard`, `CircleRecognizer::recognize` now rejects non-finite point coordinates *before* evaluating stroke bounds/inertia, and explicitly rejects non-finite endpoint gaps. Previously a NaN endpoint gap could evade the numeric `>` closure comparisons. Added direct legacy-recogniser tests for NaN/infinite x at first, middle, and last points, plus an open-C negative test. The test constructs invalid input through the public `Stroke::addPoint` API (its implementation appends the point); `getPointVector()` is const and is not mutated. **GitHub commits only; C++ compilation and tests are not verified.** On Windows UCRT64, compile and execute `ShapeRecognizerRobustness` tests, then all unit/GTK suites; inspect whether any bounds cache or upstream ingestion path normalises invalid points. Do not merge without verification.


### Phase 6 follow-up: smooth-loop input validation (2026-11-04)

On isolated branch `wip/phase6-chatgpt-smoothloop-finite-guard`, the least-squares circle/ellipse entry point now checks all raw stroke coordinates before asking `Stroke` for snapped bounds. It also rejects non-finite accumulated path length, endpoint gap, and coordinate sums before fitting. This complements the legacy inertia-circle validation already added on the parent branch. Direct regression tests cover NaN, positive/negative infinity at first/middle/last samples and finite but enormous coordinates that overflow path length. **These are committed source and tests, not a successful native build/test result.** Run the `ShapeRecognizerRobustness` unit target and full Windows UCRT64 unit/GTK suites, including 96/120 DPI and real stylus negative cases, before integration. Do not alter the real user profile.


### Phase 6 follow-up: generated-outline bounds

Branch `wip/phase6-chatgpt-segment-overflow-guard` follows `wip/phase6-chatgpt-smoothloop-finite-guard` without touching the integration branch. The legacy circle generator previously converted `2 * radius` to `int` before imposing a minimum, which is undefined for out-of-range floating-point values and can create an unbounded number of points for very large but finite diagrams. The smooth-loop generator similarly converted before clamping. Both paths now clamp the floating-point segment count *before* conversion (legacy 24–512, smooth 36–128). Added direct recogniser regression tests asserting bounded output and closed contours for a large circle and rotated ellipse. These tests have not been compiled or executed; Windows MSYS2 UCRT64 unit and GTK verification remains required. Inspect whether 512-segment circles retain sufficient visual smoothness at extreme zoom before integration.


### Phase 6 follow-up: least-squares arithmetic overflow guard (2026-11-04)

Isolated branch `wip/phase6-chatgpt-fit-arithmetic-guard` adds explicit finite checks to the 3×3 normal-equation solver, pivot/output coefficients, ellipse determinant and inverse-axis coefficients, and ellipse RMS decision. Even entirely finite stylus coordinates can produce infinite products or fourth-order sums in the fit; NaN comparisons previously risked admitting invalid ellipse coefficients. Added direct smooth-loop regression cases at radii `1e80` and `1e105` to exercise overflow in ellipse fourth moments and circle normal equations respectively. This is a defensive rejection of physically implausible geometry, not a change to ordinary recognition tolerances. **Source committed, no native compilation or tests run.** Before integration, compile under MSYS2 UCRT64, run `ShapeRecognizerRobustness` and all unit/GTK tests, and check normal-size rotated ellipses still fit. Do not touch the user's profile.

### Phase 6 follow-up: legacy inertia arithmetic validation (2026-11-04)

On isolated branch `wip/phase6-chatgpt-legacy-inertia-finite-guard`, the legacy circle path now rejects non-finite inertia mass, centre, tensor components, radius and determinant before scoring or generating an outline. `scoreCircle` returns infinity rather than zero for invalid/non-positive divisors, so invalid arithmetic cannot appear to be a perfect circle. A direct regression checks finite radius-`1e80` and radius-`1e105` sampled circles, which can overflow weighted inertia despite finite input points and path length. This follows `wip/phase6-chatgpt-fit-arithmetic-guard`. **Source committed but neither compiled nor executed.** Windows UCRT64 must compile and run `ShapeRecognizerRobustness` and the full unit/GTK suites; additionally inspect ordinary small/large circles and check any `Inertia` implementation-specific behaviour before integration.



### Phase 6 follow-up: generated-loop finite-vertex validation (2026-11-04)

On isolated branch `wip/phase6-chatgpt-loop-outline-safety`, both legacy and smooth circle/ellipse generators now check the final page-space vertex coordinates before appending them to a `Stroke`. A finite centre and finite radius/axis length do not necessarily yield finite coordinates after rotation and translation, so invalid output must be rejected rather than saved. The existing large-circle and rotated-ellipse regression cases now also assert all output vertices are finite. This branch follows `wip/phase6-chatgpt-legacy-inertia-finite-guard` and does not modify shared integration branches. **No C++ build or executable tests were run.** Before integration, run Windows MSYS2 UCRT64 compilation and `ShapeRecognizerRobustness`, full unit and GTK suites, and manual stylus tests. Review whether the production geometry pipeline can ever supply centre/axes near floating-point limits; this change is a defensive output invariant, not a claim of reproduced user-facing failure.
