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
| 2 | 1, 2, 4, 10 | not started |
| 3 | 3, 6, 8 | not started |
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

### Known gaps (Phase 1)

- Verified (final code): build exit 0; units 446 passed; GTK 94/94 at 120 DPI and at 96 DPI; real profile SHA-256 and mtime unchanged. Native captures at 1280 and 800 (120 DPI) show the status; dark mode, High Contrast, 96 DPI captures and keyboard checks of the status are NOT TESTED.
- Not covered by an automated test: the new-document reset through `Control::replaceDocument`, and the recovery wiring in `XournalMain` (needs the dialog flow). The model rules for both are unit tested.
- `beginSave` / `beginExport` stay "running" if the job's completion never fires (no window, scheduler stopped).
- The autosave attention item is matched by its translated text; a locale change mid-session would not match.
- A brand-new empty document shows an amber "Not saved yet"; consider a calmer severity while there is nothing to lose.
- Export reporting covers "Export as PDF" only. `CustomExportJob` (image export, export dialog) does not report its result yet.
- No actionable popover on the status (Save now / Retry); the tooltip carries the details.
- Visual system: remaining non-High-Contrast literals and a written type/spacing/size scale are not done.
