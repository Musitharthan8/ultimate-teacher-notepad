# Teacher UX programme: implementation record

Handoff reference between Nitro5, the home PC, Claude and Codex. Facts only; update after each phase.

## Where things are

- Branch `feature/utn-teacher-ux-program`, worktree `C:\Users\musit\Documents\utn-teacher-ux`, build dir `C:\Users\musit\Documents\utn-teacher-ux-build` (outside the repo).
- Baseline: `aafdea359` (`design/utn-teacher-first-2026-10-30` as pushed). Its tree is identical to the verified `8b69e3e1b`.
- Nothing from this branch is pushed or merged.
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
| 4 | 7, 11 | not started |
| 5 | 9, 5 | not started |
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

### Known gaps (Phase 1)

- Verified (final code): build exit 0; units 446 passed; GTK 94/94 at 120 DPI and at 96 DPI; real profile SHA-256 and mtime unchanged. Native captures at 1280 and 800 (120 DPI) show the status; dark mode, High Contrast, 96 DPI captures and keyboard checks of the status are NOT TESTED.
- Not covered by an automated test: the new-document reset through `Control::replaceDocument`, and the recovery wiring in `XournalMain` (needs the dialog flow). The model rules for both are unit tested.
- `beginSave` / `beginExport` stay "running" if the job's completion never fires (no window, scheduler stopped).
- The autosave attention item is matched by its translated text; a locale change mid-session would not match.
- A brand-new empty document shows an amber "Not saved yet"; consider a calmer severity while there is nothing to lose.
- Export reporting covers "Export as PDF" only. `CustomExportJob` (image export, export dialog) does not report its result yet.
- No actionable popover on the status (Save now / Retry); the tooltip carries the details.
- Visual system: remaining non-High-Contrast literals and a written type/spacing/size scale are not done.
