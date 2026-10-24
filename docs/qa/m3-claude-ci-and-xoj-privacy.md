# Claude review batch 1: Windows CI cause and the `.xoj` export leak

Verified head when started: `a02b9fb6e` (remote and local identical). Branch: `feature/utn-claude-m3-review`.

## 1. Windows CI failure

Runs `40047688538`, `40052366074` and `40054801746` failed the step "Run GTK integration tests" with
`ClassroomWorkflow.teacherRailShowsCaptionsBeneathIconsAndPreservesTool`.

* Log evidence for the latest run (`a02b9fb6e`): `ClassroomWorkflowTest.cpp:235` `allocated 1008, expected 1280`, then
  twelve `:243` caption-visible failures. Later in the same test a request for **1024** also gave **1008**, so the runner
  desktop is about 1024 px wide and clamps every larger request. This is a constraint of the runner, not of the product.
* The product behaved correctly: below 1080 px the rail is intentionally icon-only (`MainWindow::updateCompactLayout`).
  The test expected the wide behaviour from a window that was really 1008 px wide.
* Fix (commit `96fca9ed0`): the decision is now `utn::layout::nextCompactLayout` / `railLabelsVisible`
  (`src/core/gui/ResponsiveLayout.h`) and is tested at fixed logical widths without a monitor. The widget tests check the real
  label visibility three ways: at the width the desktop actually allocates (expectation derived from the allocation),
  at fixed logical widths through the real `updateCompactLayout` (so the wide behaviour is exercised on every runner), and through the
  real Workspace menu buttons. Nothing is skipped; no width is hardcoded for the runner.
* Local evidence (this machine, Debug, MSYS2 mingw64): `ResponsiveLayout.*` 13/13; `ClassroomWorkflow.teacherRail*` and
  `workspaceMenu*` 4/4; full `test-units` 344/344 and `test-gtk-integration` 45/45 at the time of the commit (before the
  `.xoj` change: 340 and 45).
* Limitation: this machine's monitor is larger than 1024 px, so the clamped-allocation path is covered by the fixed-width test and by
  GitHub CI, not by a local run with a 1008 px window. CI result for `feature/utn-claude-m3-review` is recorded below once available.

## 2. `.xoj` compatibility export leaked teacher content (privacy, found by reading)

`Export As → Xournal (Compatibility)` (`CustomExportJob::run`, `exportTypeXoj`) skipped the export dialog and ran
`XojExportHandler` with no audience filter. `SaveHandler::visitPage` wrote every layer and `prepareSave` wrote the page-1 thumbnail
(rendered from every visible layer).

* Red test (`StudentXojExport.*`, before the fix): `PRIVATE_TEACHER_KEY` found at offset 566 and `HIDDEN_ANSWER_KEY` at offset 760 of the
  decompressed `.xoj` XML.
* Fix: `SaveHandler::setExportAudience`, `includeLayer()`, and thumbnail omitted for Student. `XojExportHandler` is a Student
  copy by default. The native `.xopp` save is unchanged (default Teacher, complete private lesson; covered by
  `nativeSaveStillWritesTheCompletePrivateLesson` through a reload). A page whose layers are all private still writes one empty
  `<layer>` so old Xournal can read it. The file chooser entry is renamed "Xournal (Compatibility, student copy)".
* This is layer filtering, **not** redaction. A revealed Answers layer is part of a Student copy by design.

## Still open from the privacy audit (not fixed in this batch)

* No test runs `CustomExportJob` end to end from the dialog result to bytes (dialog default is tested; job wiring is read, not run).
* `PrintHandler` always filters to students (read, safe), but its page rendering function is file-local, so print is not unit tested.
* Teacher PDF copies include only *visible* layers; hidden Answers are omitted from Teacher copies too. The dialog text says "private content
  permitted" but does not say hidden layers stay hidden.
