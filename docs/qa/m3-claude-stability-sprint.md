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
SHA-256 `ce11c412d85a3722199194b046fdb2832a346d265db394a7a3c16ce81b9497d9`, built from `7ff8a19a9` (later commits change only
tests and documents). `RelWithDebInfo` with `-DUTN_LOCK_AUDIT=ON`: it logs any document-lock wait over 50 ms and any
nested acquisition, with a stack, through GLib. It is unsigned. Extract it completely and run `bin\UTN.exe`. It started and
wrote `utn-perf.log` in a 12-second smoke run on this machine; nothing else was exercised by hand.
