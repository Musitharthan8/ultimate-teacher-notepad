# Stability sprint for the 31 October demonstration

Baseline: `2ef6b1da0669963f87772f24ed2fe2cd4fd08c65` (`feature/utn-m3-reviewed-ui-candidate`, PR #26).
Branch: `feature/utn-claude-stability-oct31`. Nothing here has been run on a physical stylus; see "Hardware only".

## 1. A correction to earlier evidence

`m3-claude-stylus-investigation.md` reported 0 nested lock acquisitions and 0 slow waits in a soak that "runs the real
`AutosaveJob` on the scheduler while worker threads render". That was not what ran. The GTK fixture builds `Control` and
`MainWindow` without calling `Scheduler::start()` (only `XournalMain` does), so no render job and no autosave job ever
executed in those tests. The lock audit therefore saw only the main thread. Both soak tests now start the scheduler.
With the scheduler running, the baseline shows the freeze (next section).

## 2. Root cause that reproduces the field symptom

Autosave rasterised every partially erased stroke while holding the shared `Document` lock (`AutosaveJob::run` ->
`SaveHandler::prepareSave` -> `rasterizeMaskedStroke`, per erased stroke). Pen-up needs the exclusive lock
(`StrokeHandler::onButtonReleaseEvent`, `doc->lock()`), so it waits for the whole raster. The wait grows with the number of
erased strokes, hence with session length. It was 360 ms on average and up to 3.7 s in the stress session.

Two things made it worse and were fixed with it: every pointer event of an erase added an erasure capsule (so each erased
stroke got slower to draw, to save, and finally uneditable), and every erase gesture ended with a whole-page re-render on a
render thread that holds the shared lock for the page.

### Measurements

Same test, same machine, Debug build with `-DUTN_LOCK_AUDIT=ON`, scheduler running, Student View open, autosave every
15 iterations, 3 pages, mixed pen / highlighter / default eraser, undo and redo every 11th iteration. "Baseline" is the
reviewed candidate plus measurement only (`diagnostics/utn-claude-stability-baseline-instrumented`).

2400 iterations (about 31 000 eraser pointer events):

| | baseline | fixed |
| --- | --- | --- |
| pen-up wait for the exclusive lock: mean / p95 / max | 46.6 / 1247 / 2743 ms | 0.39 / 1.9 / 58 ms |
| slowest single iteration | 5048 ms | 400 ms |
| p95 of the last third / first third | 1411 / 205 ms (6.9x) | 125 / 85 ms (1.5x) |
| autosave: shared lock held, mean / max | 1019 / 3687 ms | 2.4 / 5.8 ms |
| exclusive lock wait, any thread, max | 3686 ms | 107 ms |
| lock waits over 50 ms | 292 | 4 |
| eraser event p95 / max | 30.5 / 3687 ms | 0.8 / 46 ms |
| erasure capsules on one stroke, max | 66 | 26 |
| render job max | 273 ms | 59 ms |
| Student View share of UI time | 42% | 14% |

600 iterations: pen-up wait p95 112 -> 0.03 ms (max 1528 -> 20 ms); slowest iteration 1642 -> 86 ms; lock waits over 50 ms
43 -> 0.

How to reproduce (Debug build dir with `-DUTN_LOCK_AUDIT=ON`):

    UTN_SOAK_STROKES=2400 ./test-gtk-integration.exe --gtest_filter='ClassroomWorkflow.heavySession*' --gtest_output=xml:out.xml

The properties in the XML (`perf_*`, `heavy_*`) are the numbers above. Run it on the baseline branch for the left column.

The same 2400-iteration session in an optimised build (`RelWithDebInfo`, lock audit on, the build type of the portable
bundle):

| | baseline | fixed |
| --- | --- | --- |
| pen-up wait for the exclusive lock: mean / p95 / max | 41 / 125 / 1582 ms | 0.24 / 0.03 / 31 ms |
| slowest single iteration | 3337 ms | 435 ms |
| p95 of the last third / first third | 1329 / 173 ms (7.7x) | 101 / 65 ms (1.6x) |
| autosave: shared lock held, mean / max | 888 / 1743 ms | 1.2 / 4.2 ms |
| lock waits over 50 ms | 253 | 0 |
| longest lock wait | 1631 ms | 42 ms |
| Student View frame mean / share of UI time | 139 ms / 42% | 31 ms / 14% |

The optimised build is not much better than Debug for the baseline: the time is in cairo and in waiting, not in our
unoptimised code. A hardware session is still the only proof that this is the owner's freeze.

## 3. What changed, by commit

| Commit | Change |
| --- | --- |
| `43b5b88db` | `utn::perf` (`UTN_PERF=1`): pen-up, lock waits, render, autosave, Student View frame, eraser, recogniser, UI-stall heartbeat, process memory and GDI/USER/kernel handles. Off by default. |
| `ce383fde0` | Erasure capsules merge while every sample stays within 2% of the radius (at most 256 per capsule). Erase repaint range is the capsule, not the stroke box. `prepareSave` snapshots erased strokes; `SaveHandler::resolveDeferredStrokes()` rasterises after the lock is released, with coarser retries. An undrawable stroke no longer fails every later save: teacher copy keeps its usable points with a warning, student copy leaves it out. `SaveJob` stops before moving the previous file to the backup name when preparation reported an error. |
| `c82514f76` | `PenInputHandler` edge scrolling: a GLib timeout instead of a never-joined `new std::thread` that captured `this` and its locals by reference. |
| `730a9f3b0` | Spotlight and Curtain are painted on the Student View frame (one shared painter). Ghosting of hidden answers is opt-in. "Reveal all answers" needs a second press while the class may be watching; every reveal offers "Hide again". Student View frame is cached and rendered by a cost-based throttle; shrinking the audience redraws at once. |
| `924637da8` | Smart Shape: 32-glyph handwriting corpus and robustness tests. No threshold changed. |
| `47b3f3790` | Autosave records the undo position on the main thread (the job read the undo list from its worker thread). |
| `a3734c54c` | Repeated scrubbing over the same ink adds no capsules (5% slack in "already erased"); Student View redraw throttle tightened; soak tests start the scheduler. |
| `d0bc4687c` | The heartbeat starts with the app. A watchdog thread writes `utn-perf.log` (temp folder, or `UTN_PERF_FILE`) when the UI thread has not responded for 2 s, and when it recovers. |
| `7ff8a19a9` | No whole-page repaint at the end of an erase gesture unless elements were replaced. |
| `54c0466f5` | Tests for opt-in ghosting and for tooltips. |

## 4. Presentation and privacy behaviour now

* Curtain and Spotlight cover the Student View frame as well as the teacher canvas (student Curtain is fully opaque, the
  teacher's stays at 88% so the page can still be read). Tested pixel by pixel on the student frame.
* Escape, tapping the active mode again, the banner button, the Present menu and any tool switch all leave the overlay
  (these existed and have tests); the banner is always at the top of the page area.
* Hidden-answer ghosting on the teacher canvas is off until the teacher turns on "Show hidden answers faintly to me" in the
  Students menu, and still needs a second monitor.
* "Reveal all answers" while Student View is Live or the screen is duplicated: first press shows a toast, a second press
  within 5 seconds reveals. "Reveal on this page" is one press. Both show "Hide again". No modal dialog.
* Anything that shrinks what students may see is drawn immediately; other changes are rate limited by their own cost.

## 5. Tests added

Unit (376 pass): `SaveIntegrity.*` (snapshot at prepare time, undrawable stroke teacher and student, no usable points),
`EraserCoalescing.*` (compactness, tolerance bound of every sample, sharp turns, reversal, radius, repeated scrubbing,
a pass that reaches new ink still erases), `HighlighterErasing.*` (hole survives save and reopen; student copy has no
editable original and no vector stroke), `PerfCounters.*`, `ShapeGlyphs.*`, `ShapeRobustness.*`.
GTK (67 pass): Curtain and Spotlight on the student frame, reveal-all confirmation and Hide again, opt-in ghosting, no
tooltip on the panes around the worksheet, and the heavy session (fails if the last third of a session is more than twice
the first plus 50 ms, if Student View takes more than 20% of UI time, or on any nested lock).
`test-lock-audit` 6 pass.

## 6. Smart Shape

Measured on 2880 handwriting samples (32 glyphs x 3 sizes x 3 sloppiness x 10): nothing neat or typical became a shape; one
small sloppy "D" became a circle (1 in 90 for that glyph, 1 in 2800 overall). l, I, "/", "-" become straight lines about
60% of the time, which is allowed. "o", "O" and "0" become circles, and cannot be told from a circle by geometry; the
recogniser is an explicit mode for that reason. Recognition took at most 0.8 ms; 8000-point strokes under 50 ms. Recall
(existing corpus, unchanged): circle 100%, square 81%, triangle 76%, line 69%, rectangle 67%, ellipse 5% (deliberately
left alone). I did not raise recall: every route I considered would trade it for false positives, and the glyph corpus is
still synthetic.

## 7. Not done, hypotheses, blockers

* A hardware freeze may have another cause. The one found here needs erased strokes plus autosave; if the owner's 34-minute
  session had neither, it does not explain it. The log files below will say.
* A full-page render still holds the shared lock for the whole page (zoom, scroll, undo and redo trigger them). Debug
  build: 59 ms at the 2400-iteration point; Release numbers above. Making render lock-free needs a snapshot design.
* Highlighter erase on a PDF background and PDF vector export parity are covered by existing tests only.
* Esc does not leave Blank (the "Show page" button does). Presentation mode (single page, zoom disabled) is saved in the
  settings and survives a restart; it is toggled from Present.
* The repeated-tap-opens-settings behaviour and the Prepare / Teach / Mark / Present flow were not re-verified natively.
  The product deck was not reachable (`Requested entity was not found` through the Drive connector).
* Handwriting to text, OCR, offline dictionary, GIF/video and PDF merging remain missing or unverified (see
  `m3-feature-status.md`).

## 8. Hardware only

1. Run the portable build (below) for 30 minutes of real writing. Before starting:
   `set UTN_PERF=1` and `set UTN_PERF_FILE=C:\Temp\utn-perf.log`, then start `bin\UTN.exe` from that prompt. Send the file.
   It has a summary every 30 s and a line "UI THREAD NOT RESPONDING" with the counters if the window ever freezes for 2 s.
2. Erase a lot, highlight, keep Student View open on a second screen, autosave on its default interval.
3. Palm rejection, pen button and eraser end, high DPI, touch target size.
4. Projector in Extend and Duplicate modes; unplug it during Live, Freeze and Blank. Screen-share the teacher window with
   ghosting off and confirm no hidden answer is visible.
5. Curtain and Spotlight with Student View on the projector.

## 9. Do not demonstrate

Stroke recognizer on cursive handwriting; heavy scribble-erasing on a very dense page with Student View open (until the
hardware run above); handwriting to text; OCR; the dictionary offline.

## 10. Reviewing this branch (for Codex)

    git -C C:\Users\musit\Documents\ultimate-teacher-notepad log --oneline 2ef6b1da0..feature/utn-claude-stability-oct31
    git -C C:\Users\musit\Documents\ultimate-teacher-notepad diff 2ef6b1da0 feature/utn-claude-stability-oct31 --stat

The branch is checked out in `C:\Users\musit\Documents\utn-claude-stability-oct31` (do not edit it; make a worktree).
Build: MSYS2 mingw64, `cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DUTN_LOCK_AUDIT=ON -DENABLE_GTEST=ON -DPACKAGE_ARCH=x86_64`,
targets `test-units test-gtk-integration test-lock-audit xournalpp`. Check in this order:

1. `ce383fde0` `SaveHandler::resolveDeferredStrokes` and `XmlDeferredStrokeNode`: is there any path that writes the tree
   without calling it? (`saveTo` both overloads do; `CrashHandler`, `XojExportHandler` and `CustomExportJob` go through
   `saveTo`.) Does a student export ever see the original of an erased stroke?
2. `Stroke::addErasure`: the 2% bound, and that `erasureChain` can never apply to a stroke whose capsules were changed by
   a transform (`move`, `scale`) or by undo. `EraseHandler.cpp` `alreadyErased` slack (5%).
3. `ToolStudentView`: the throttle, `studentAudienceChanged()` (never blocks; `doc->getPage`, not `getCurrentPage()`), the
   frame cache with HiDPI, freeze copy, and that nothing but `refreshLiveFrame` renders.
4. `RevealActions` and the "may watch" flag (Live or Same screen).
5. `PenInputHandler::startEdgeScroll`: stops on pen return, on lift, on destruction.
6. Run `UTN_SOAK_STROKES=2400 ... heavySession*` on this branch and on the diagnostics branch and compare.

Please try to break: erase while autosave is running on a masked stroke that is also being moved; reveal during Freeze;
Curtain while Student View is Blank; delete a page while Student View is open.

## 11. Portable build for the hardware run

`C:\Users\musit\Documents\utn-claude-stability-oct31-release\utn-release\UTN-0.2.1-beta-Windows-x64-Portable.zip`
SHA-256 `6bc46937ffbb889cff6b9b189da096322e1c9e248ec9a7e508383161ae13a329`, rebuilt after the review corrections from
`a47afdf68` (the earlier bundle, `ce11c412...`, is superseded). `RelWithDebInfo` with `-DUTN_LOCK_AUDIT=ON`: it logs any
document-lock wait over 50 ms and any nested acquisition, with a stack, through GLib. It is unsigned. Extract it
completely and run `bin\UTN.exe`. It starts and writes `utn-perf.log` (a 75-second smoke run showed the 30-second
summaries); nothing else was exercised by hand.

## 12. Corrections after the independent review (Codex, on `944f21798`)

Supersedes sections 2 and 3 where they differ. Commits `b85fc2062`, `46b5d3b5d`, `a47afdf68`.

1. **Compatibility export held the Document lock** (`CustomExportJob::run`). The shared lock now ends right after
   `prepareSave`; `SaveHandler::saveAtomically` draws and writes with no lock. The prepared tree owns snapshots of erased
   strokes and reference-counted image surfaces (same ownership as autosave and `SaveJob`). Test: the real job runs while the
   test thread draws, erases and queues autosaves; the worst edit wait was 559 ms with the old lock scope (mutation
   checked) and stays under the 250 ms bound now. `SaveIntegrity.rasterWorkHappensInResolveNotInPrepare` counts calls to
   `rasterizeMaskedStroke` (new `rasterizeMaskedStrokeCallCount()`), and `editingIsNotBlockedWhileADenseDocumentIsSaved`
   saves 150 erased strokes on one thread while another edits.
2. **A student copy never succeeds with an annotation missing.** `resolveDeferredStrokes` now sets the error (not a
   warning) when a student copy's erased stroke cannot be drawn, before any file is created. Erased ink is never written
   back into a student copy. `saveAtomically` writes `<file>.utn-tmp` and replaces the destination only with a whole file;
   on any failure the existing file is untouched and the temporary file is removed. Callers: `CustomExportJob` (student
   `.xoj`) uses it; `SaveJob`, `AutosaveJob`, `CrashHandler` and the CLI are teacher saves and keep the "keep the ink,
   warn" fallback. Tests: a failed student export keeps the previous file byte for byte and leaves no temporary file;
   whole-file replacement; an unwritable folder; the rasterisation failure itself.
3. **Frozen Student View kept revealed answers.** The frame records which layers students could see; every redraw request
   re-checks them. If students may now see less (answers hidden on this page or everywhere, a layer made private,
   invisible or removed, Curtain or Spotlight switched on after the freeze, or the check cannot be made) the frozen bitmap
   is destroyed and the screen goes blank immediately, with a toast; "Show the page again" resumes live. Ordinary changes
   keep the freeze. Test `frozenStudentViewNeverKeepsWhatStudentsMayNoLongerSee` asserts real Student View pixels for each
   transition, Blank over Freeze, ordinary ink keeping the freeze, and resizing. Projector reconnect cannot be simulated
   here: monitor events only change the pill and the opt-in ghosting, and do not touch the frame (hardware task).
4. **Measurements.** `UtnPerf` statistics now carry two labelled populations: *all* (every sample) and *window* (the last up
   to 65536), so mean, p95 and max are never mixed (before, p95 was over the last 512 only). The XML properties are named
   `*_all_*` and `*_win_*`. The soak asserts the temporary folder works, that autosave wrote a file that reopens, and
   records UI stalls (heartbeat lateness above two Windows timer ticks), memory, GDI/USER objects and handles. The soak now
   queues an autosave only when none is pending, like the real timer; before, it queued a backlog of about 160 jobs that
   teardown waited minutes for (that, not a deadlock, is what a stuck test process was).

### Fresh like-for-like measurements

Same test, same machine, one run each, isolated temp folders (`TMP`, `TEMP`, `TMPDIR` set to a folder under
`C:\Users\musit\utn-tmp`), lock audit on, scheduler running, 2400 iterations, Student View open. "Baseline" is
`diagnostics/utn-claude-stability-baseline-instrumented` (measurement only). Debug / RelWithDebInfo:

| | baseline | fixed |
| --- | --- | --- |
| pen-up wait for the exclusive lock, all: mean / max | 30.8 / 1221 and 28.6 / 1217 ms | 0.38 / 49 and 0.40 / 39 ms |
| same, window p95 | 117 and 89 ms | 0.29 and 0.04 ms |
| autosave shared lock held, all: mean / max | 660 / 1438 and 661 / 1328 ms | 3.2 / 11 and 0.95 / 4.2 ms |
| slowest iteration | 2239 and 2280 ms | 626 and 493 ms |
| p95 of the last / first third of the session | 1020 / 257 ms (4.0x) and 889 / 226 ms (3.9x) | 71 / 79 ms (0.9x) and 52 / 65 ms (0.8x) |
| UI heartbeat late by over 30 ms: count, window p95 | 1155 / 701 ms and 1039 / 746 ms | 167 / 152 ms and 108 / 150 ms |
| Student View frame: all mean, window p95 | 166 / 304 and 158 / 288 ms | 43 / 129 and 42 / 133 ms |
| Student View share of loop time | 47% and 46% | 14% and 14% |
| lock waits over 50 ms / longest | 226 / 1406 and 196 / 1217 ms | 3 / 98 and 2 / 99 ms |
| result of the same assertions | fails (slower over time, Student View share) | passes |

Memory: private bytes started at 64 to 66 MiB in all four runs. The last sample is not a trend (baseline 84 and 441 MiB,
fixed 159 and 157 MiB) because it depends on when the allocator last released pages; the maxima were 205 to 475 MiB
(baseline) and 210 MiB (fixed). Kernel handles and GDI objects did not grow in any run. I do not claim a memory fix.

### Verification on these commits

382 unit tests, 69 GTK tests (including the heavy session, compatibility export, frozen Student View) and 6 lock-audit
tests passed (Debug, lock audit on, `TMP` isolated). After the last commit (`a47afdf68`, the stall threshold) the unit and
lock-audit suites and the four soak runs above were repeated; the full GTK suite was last run on `46b5d3b5d`.

### Remaining

Unchanged from section 7, plus: no physical-stylus evidence; a full-page render still holds the shared lock for the page;
the `.utn-tmp` file of a crash mid-export is left beside the destination (harmless, the destination is intact); private
bytes in long sessions are unexplained. Do not describe the freeze as fixed until the hardware run in section 8.

## 13. Final safety corrections (second review, on `889234f09`)

Two release-blocking defects from the second Codex review. Nothing in the performance work was changed.

### 13.1 A failed export must keep the previous file

Cause: `SaveHandler::saveAtomically` finished with `Util::safeRenameFile(temp, destination)`, which calls
`fs::remove(destination)` before `fs::rename`. If the rename then failed, the last valid copy was already gone.

Fix, in `src/util/PathUtil.cpp` (new functions; `safeRenameFile` is unchanged because its other callers were not audited
for this change) and `SaveHandler::saveAtomically`:

- `Util::createExclusiveSiblingFile` makes the temporary file next to the destination with exclusive creation
  (`CreateFileW(CREATE_NEW)` on Windows, `open(O_CREAT | O_EXCL)` elsewhere). The name contains the process id, random
  bits and a counter, so two saves, threads or processes never share one, and a leftover `*.utn-tmp` file from an
  interrupted save (old or new naming) is neither used nor removed.
- The file is written and closed completely before anything touches the destination.
- `Util::replaceFileKeepingOld`:
  - Windows, destination exists: `ReplaceFileW` with a private backup name and `REPLACEFILE_WRITE_THROUGH`. The documented
    failure modes of `ReplaceFileW` without a backup name can leave the destination missing; with one, both files keep
    their names, and if the destination is nevertheless missing afterwards it is moved back from the backup. The backup is
    deleted after success.
  - Windows, no destination: `MoveFileExW` without `MOVEFILE_REPLACE_EXISTING`, so a file that appeared meanwhile makes it
    retry through `ReplaceFileW` instead of being overwritten without protection.
  - Elsewhere: `rename(2)`, which replaces in one step on one filesystem.
  - Never throws; errors become the message returned to the caller.
- Every step of `saveAtomically` is inside a try/catch; only the save's own temporary file is removed on failure.
- Concurrent saves to one destination: each takes a ticket (per destination, per process). Replacement is serialised, and
  a save whose ticket is older than the newest one already committed does not replace the file
  (`wasSupersededByNewerSave()`). Saves from other processes to the same file are not coordinated.

What this does and does not guarantee: replacement is as atomic as the filesystem makes `ReplaceFileW` / `rename`; I did
not test power loss, network shares or antivirus holding the file, and I make no claim about them. The teacher's own
Save (`SaveJob`) and autosave still use their older strategy (backup file `name~`, and rename with `swap`), which is not
part of this change.

Tests (`SaveReplace.*`, 13, in `test/unit_tests/control/SaveIntegrityTest.cpp`): existing destination replaced and nothing
left behind; new destination; Unicode path; locked destination (opened with no sharing, Windows) fails with the old bytes
intact and nothing left, then succeeds once released; simulated replace failure keeps the old file byte for byte;
unexpected exception is reported, not thrown; missing folder; foreign and interrupted temporary files are untouched;
1600 temporary names from 8 threads never collide; eight concurrent saves leave one loadable file; an older slow save
(held inside the write) does not overwrite a newer completed one; direct `replaceFileKeepingOld` cases. The earlier
failed-student-export test (old file kept, no temp file) is unchanged.

### 13.2 Frozen Student View after real visibility changes

Cause: the freeze re-checked itself only when a page-change notification reached `requestRedraw`. The earlier test sent
that notification by hand. Teacher-only changes, layer removal and some structure changes had no such path, and the check
compared only whether an overlay was present, so Spotlight to Curtain kept the less restrictive picture.

Fix:

- `model/StudentVisibility.{h,cpp}`: one listener registry. `Layer::setVisible`, `Layer::setAudience` and
  `XojPage::removeLayer` announce from the model (the listener must defer: these run under the Document lock and on any
  thread). `LayerController::fireLayerVisibilityChanged` / `fireRebuildLayerMenu` and `ToolHandler`'s
  `setSpotlightEnabled` / `setCurtainEnabled` / `clearPresentationOverlay` announce synchronously after their locks are gone.
- `ToolStudentView::onStudentVisibilityChanged`: synchronous path checks the frozen frame on the UI thread at once and
  redraws urgently if the student-visible layers or the overlay changed; the deferred path queues a high-priority idle
  check, and only when a frozen frame exists.
- `drawStudentView` checks the frozen frame before painting it and paints black if it is unsafe, then drops it. A delayed
  redraw, an exposure or a timer therefore cannot show it.
- The frozen frame records the overlay it was drawn under (`OverlayKind`). Any other overlay blanks it: Spotlight to
  Curtain, Curtain to Spotlight, and switching an overlay on. Switching the overlay off keeps the freeze, because the frozen
  picture was already covered at least as much. There is no separate "scope" for an overlay in the code, so none is checked.
- `XojPage::getLayerRemovalCount()` makes any layer removal on the frozen page unsafe, even if a new layer reuses the address.
- Revealing answers, new ink, a new layer, a pen switch and hiding something that was never in the picture keep the freeze.

Tests (GTK, `ClassroomWorkflow.*`): nothing is announced by the test; each action calls what the widget calls.
`frozenStudentViewIsBlankedByTheRealLayerAndOverlayActions` (layer checkbox path `LayerController::setLayerVisible`, hide
all layers, `hideAnswersFromUi`, a teacher-only change with only the model setter, a delayed redraw queued first and eight
rounds of notifications afterwards), `frozenStudentViewIsBlankedWhenAnOverlayChanges` (Curtain on, Spotlight on, Spotlight
to Curtain, Curtain to Spotlight, overlay off keeps the freeze, through `ToolPresentationKit::runClassroomAction`),
`frozenStudentViewIsBlankedWhenTheAnswersLayerIsRemoved` (`deleteCurrentLayer`),
`ordinaryChangesKeepTheFreezeAndLiveAndBlankStillWork`. They read the state before the main loop runs again and assert the
pixels the Student View draws. Mutation checks: with the new listener calls and the draw check disabled the teacher-only
case fails; with the overlay rule reverted to "an overlay was already there" Spotlight to Curtain and Curtain to Spotlight
fail. `StudentVisibility.*` (3 unit tests) cover the announcements.

Honest note: the layer-hide checkbox path was already reaching the freeze through the canvas-changed listener (the page view
re-renders), so that case passes even without the new listener; the new calls make it independent of that.
Not covered: moving an object from a visible layer to a private one, projector reconnect (hardware), element-level changes.

### 13.3 Verification on these commits (`2db0b453f`, `6b231631b`)

Debug with lock audit and RelWithDebInfo with lock audit, `TMP`/`TEMP`/`TMPDIR` isolated:

- Debug: 398 unit tests, 73 GTK tests, 6 lock-audit tests passed. One earlier full GTK run had 2 layout failures
  (`inspectorWorkspaceModesPreserveToolStateAndPersist`, `answerBoxSettingsPopoverFitsAShortWindow...`: paned position and
  popover height) in tests that run before any of the new ones and pass alone and on the repeat; I did not find the cause
  (window geometry on this desktop is the suspect) and I do not call it fixed.
- RelWithDebInfo: 398 unit tests, 73 GTK tests passed.
- Heavy soak, 2400 iterations, Student View open: six Debug runs and four RelWithDebInfo runs, all passed their assertions.
  Means were stable across runs (Debug: pen-up lock wait 0.20 to 0.42 ms, autosave shared lock held 2.7 to 3.3 ms mean and
  at most 11 ms, Student View frame 43 to 45 ms mean and 14% of loop time; RelWithDebInfo: 0.34 to 0.47 ms, 0.9 to 1.2 ms
  mean and at most 7.7 ms, 39 to 46 ms, 14%). Last/first third p95 ratio stayed below 1.
- Occasional spikes: one Debug run had a render job of 806 ms, an erase event of 855 ms, a UI stall of 1.2 s and a longest
  lock wait of 803 ms together, and one RelWithDebInfo run had a pen-up wait of 479 ms and another a UI stall of 1.5 s.
  The Debug spike hit every thread at once, which points at the machine (other applications are running), but I cannot rule
  out the known weakness that a full-page render holds the shared lock for the whole page (section 7). They did not occur in
  the first sampling in section 12 (longest wait 98 to 99 ms) and I did not run the previous commit again to compare, so
  the comparison is of means only.
