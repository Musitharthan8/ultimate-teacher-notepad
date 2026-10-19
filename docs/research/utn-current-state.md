# UTN current-state audit

Audit date: 19 October 2026. Source inspected: `feature/utn-classroom-ui-recovery`, HEAD `f802b7b28ab745a5f82885214cc34be64d28f9ec`. This is a source and documentation audit; historical test results below are attributed to their records and are not fresh executions. Remote/PR verification is recorded separately by the coordinating audit. The pre-existing untracked `install/` directory was left untouched.

## Architecture and implementation

UTN extends Xournal++ rather than replacing its document engine. Root `CMakeLists.txt` declares project version 1.3.8+dev, C++20, GTK 3 (`gtk+-3.0 >= 3.18.9`) and Poppler GLib (`>= 0.41.0`). Model/view/control, tool handlers and undo actions remain under `src/core`; resources and GTK UI definitions remain separate. API names resembling GTK 4 in adapted code must not be taken as evidence of a GTK 4 migration.

| Area | Implemented evidence | Limits / verification needed |
| --- | --- | --- |
| Teacher shell | `gui/toolbarMenubar/ToolUtnContextBar.cpp`, `gui/toolbarMenubar/ShapePicker.cpp`, `gui/toolbarMenubar/ToolMenuHandler.cpp`; recent commits add keyboard shortcut tooltips, explicit shell colours, 24 px split arrows, narrower properties controls and scrollable tool rail | Recent layout work needs actual short-window/high-DPI/tablet verification. The current head's tool-rail scroll change is already implemented and should not be proposed anew. |
| Pen | `control/tools/StrokeHandler.cpp:58-169,383-396` integrates the upstream stabilizer, pressure-aware points, finalization and zoom conversion | Source presence establishes support, not measured latency, palm rejection or device compatibility. |
| Eraser | `control/tools/EraseHandler.cpp` has partial ink, whole-stroke and non-stroke object deletion; sampled swept discs bridge sparse input, pressure-aware broad phase, hidden-layer protection, gesture undo and original element ordering | Partial erasing removes centreline intervals and is explicitly documented as over-erasing thick ink; whole-stroke pressure hit test uses a conservative endpoint maximum that differs from outgoing-segment rendering. Whiteout is handled outside this handler. |
| Text | `control/tools/TextEditor.cpp` supports fonts, alignment, line spacing, editable boxes and automatic list continuation; `model/Text.cpp`, `view/TextView.cpp`, `undo/TextBoxUndoAction.cpp` preserve model/render/history integration | No verified complete spell-checking implementation. Unicode rendering and list tests do not prove every IME or multilingual editing flow. |
| Shapes | `control/tools/BaseShapeHandler.cpp` processes Shift/Control/Alt and finalizes through existing undo/model paths; `gui/toolbarMenubar/ShapePicker.cpp` exposes choices; recognizer tests cover circles, ellipses and rectangles | Two square-versus-circle pipeline tests remain disabled (`CircleRecognizerTest.cpp:164,185`). Do not claim universal recognition reliability. |
| Answer Box | Text model/rendering and contextual style controls provide border, fill and editable content; object eraser treats a box as one non-stroke element | Previous clipped-border redraw bug is recorded as corrected; current tests include real raster invalidation at multiple zooms. Do not reintroduce that fix as new work. |
| Answer layers | `model/LayerAudience.cpp` explicitly encodes everyone/teacher/answers; legacy names migrate, loaded answers start hidden, writable-layer helpers avoid hidden answers | Privacy depends on consistent use of audience filtering throughout new render/export paths. |
| Student View | `gui/toolbarMenubar/ToolStudentView.cpp:44-72,142-197,252-270,364-378,418-548` renders student-filtered frames and supports live, freeze and blank states, monitor changes and teacher state indicators | Real projector movement, same-screen presentation, hotplug and tablet behaviour still require manual validation. |
| Smart Highlighter | `control/ToolHandler.h:45`, `control/tools/StrokeHandler.cpp:219-334`, `gui/toolbarMenubar/HighlightModes.h` implement freehand, straight, PDF word/line snap, underline and strikethrough | Selectable PDF text geometry is used; this is not evidence of OCR for scanned pages. |
| Marking / feedback | `gui/toolbarMenubar/ToolMenuHandler.cpp` registers classroom controls; `docs/UTN-Classroom-Workflow-Roadmap.md` identifies a marking layout, editable comments, Feedback Bank and marked-copy save/export | Anchored pins, rubrics, automatic grading and submission queues remain planned. |
| Lesson navigation / reveal | `gui/toolbarMenubar/ToolLessonNavigator.cpp` and layer audience helpers provide classroom navigation and answer visibility operations | Reordering/deleting pages and visibility changes need multi-page end-to-end checks. |
| Document management | Existing `control/xojfile/SaveHandler.cpp`, `control/xojfile/LoadHandler.cpp`, page controls and PDF export preserve XOPP/Xournal++ foundations | Scan backgrounds may use attached `.xopp.bg_*.png` sidecars; users must retain them. Multi-source merge/batch split are roadmap features, not confirmed implementations. |
| Other tools | `docs/UTN-Free-Classroom-Tools.md` describes native signature insertion, local Windows speech, existing equation/audio/image tools and online dictionary integration | Speech has a 5,000-character limit; no synchronized PDF reading. OCR needs external Tesseract, equations need local TeX. Dictionary sends the explicitly entered word to dictionaryapi.dev. |

Paths in the implementation column are relative to `src/core` unless otherwise stated. Documentation describing future work is not proof of implementation.

## Existing defect evidence

`docs/UTN-Desktop-Defect-Audit.md` records reproduced tool-switch zoom jumps, clipped property controls, Answer Box border residue after object erasing and bulky split arrows on an earlier build. It records corrections plus limited manual verification, and explicitly leaves GUI save/reopen, PDF export and tablet/high-DPI checks incomplete. Recent commits go beyond the audit with scrollable tool rail and clearer Answer Box visibility/Delete Box guidance.

`docs/UTN-Sunday-Release-Gate.md` retains the thick-stroke partial-eraser precision defect and actual-device sign-off requirements. `docs/UTN-Classroom-Workflow-Roadmap.md` is explicitly a plan and contains older status wording; its references to dictionary lookup as future work conflict with the later free-tools document. Prefer current source and the later specific feature documentation when determining availability.

## Tests and CI

`test/CMakeLists.txt` defines separate `test-units`, `test-gtk-integration` and benchmark executables. GTest is optional (`ENABLE_GTEST` defaults off); tests use installed GoogleTest unless `DOWNLOAD_GTEST` explicitly requests v1.16.0. New test files require CMake regeneration because recursive globs lack `CONFIGURE_DEPENDS` (also documented in `test/README.md`).

Meaningful existing coverage includes:

- `CircularEraserTest.cpp`: disc/segment intersections, pressure-varying partial intervals checked against dense sampling, circular object bounds.
- `EraseHandlerTest.cpp`: sparse motion sweeps, separate gestures, pressure reach, circular partial/stroke modes, undo/redo, z-order across pointer events, text/images/Answer Boxes, hidden answer protection, XOPP round trips, PDF export and rendered border invalidation.
- `LayerAudienceTest.cpp`: student visibility, explicit audience serialization, legacy migration, hidden-on-load and visible ink target selection.
- `LessonRoundTripTest.cpp`: lesson save/reopen with answers hidden.
- `ListContinuationTest.cpp`: bullet/number continuation, empty list termination, malformed and multibyte inputs.
- `ToolHandlerSettingsTest.cpp`, `TeacherToolTransitionTest.cpp`, `UtnContextLayoutTest.cpp` and GTK toolbar tests: settings/state/layout/control regressions.
- `CircleRecognizerTest.cpp`: pipeline geometry tests with two explicitly disabled square cases.

`.github/workflows/continuous-integration.yml` builds units across platform jobs and tests several locales; its Linux path also runs GTK integration under Xvfb. No dedicated sanitizer or leak-check job was identified in that workflow. `.github/workflows/utn-windows-release.yml` builds Release, runs units, packages NSIS and portable ZIP, clean-installs to a path containing spaces with MSYS2 removed from PATH, checks executable startup and PDF/PNG signatures, then uninstalls. Its push trigger currently names `feature/utn-shell` and `utn-dev`, not the inspected classroom recovery branch; workflow dispatch remains available. It does not execute GTK integration tests.

Historical evidence: the Sunday gate attributes a passing packaging run to GitHub Actions run 38704956034 / commit 5ded5d8ef, and 271 enabled local tests across 43 suites with two disabled shape cases. This audit did not rerun those checks. The local `build/test` path was absent at inspection; do not assume an existing native test executable is available there.

## Narrow recommended P1 correction

Whole-stroke erasing currently tests segment distance against the larger endpoint half-width in both `control/tools/EraseHandler.cpp`'s `wholeStrokeTouchesDisc` and `model/eraser/CircularEraser.h`'s `circleHitsStroke`. Independent rendering inspection corrects the initial interpolation hypothesis: `view/StrokeViewHelper.cpp::drawWithPressure` draws each PDF segment at its starting point's `p.z`; `model/StrokeContour.cpp` uses matching outgoing widths and the penultimate point's width for the final cap. Widths are not linearly interpolated. A large final point pressure is unused by rendering but currently widens the eraser test. `view/StrokeView.cpp` also renders highlighter strokes at fallback `stroke.getWidth()`, even if pressure data exists.

The narrow correction is an allocation-free constant-width disc/capsule test using `radius + halfWidth(a)` for outgoing segment `[a,b]`; use pressure only when the renderer does, otherwise fallback width. Keep the generic pure geometry helper unchanged: it has a separately documented conservative knot-width contract and is not the live handler path. Do not reuse the interpolated `segmentInkDiscIntervals` model for this fix. Add deterministic unused-final-pressure and interior-width-transition near-misses, true outgoing-width hits, highlighter fallback-width cases, duplicate-point dot cases and undo/redo regressions. Retain existing broad-phase rejection conservatively. Files: `src/core/control/tools/EraseHandler.cpp`, `test/unit_tests/control/EraseHandlerTest.cpp`. Acceptance: no deletion for renderer-proven misses, deletion for true hits, existing eraser tests passing. Before fresh tests this remains a source-derived defect, not a reproduced desktop result.

Scope limits: capsule testing still ignores dashed gaps, filled interiors and non-round cap differences. Single-point strokes are not drawn by `StrokeView` (which rejects fewer than two points), so preserve the existing helper's single-knot handling as compatibility rather than claiming renderer parity for malformed strokes. Two coincident points form a dot using the first pressure value. This correction does not change partial erasing, its interpolation model, or its documented thick-ink precision limitation.

## Remaining validation priorities

1. P1: establish the outgoing-width whole-stroke regression and correct only if confirmed.
2. P1: run build, units and GTK tests on the chosen development base; make Windows release dispatch an explicit review gate.
3. P1: manually exercise PDF worksheet → annotate → Answer Box → hide/reveal → Student View → undo/redo → save/reopen → PDF export, including two monitors and real stylus hardware.
4. P1: address thick-stroke partial-eraser precision through a separately reviewed persistence/rendering design; centreline clipping alone cannot represent arbitrary pixel subtraction.
5. P2: restore the two disabled shape-recognition tests with focused classification fixes, and expand visual/layout checks after short-window and high-DPI reproduction.

## Implementation addendum

The sections above describe the audited base f802b7b28, not the final patched branch. The outgoing-width handler correction is now implemented and independently reviewed. Baseline 280 enabled tests passed; both new regressions reproduced incorrect deletion, then the corrected suite passed 282 enabled units and 29 GTK tests. The GTK palette assertion was normalized for Windows path separators. Two existing recognizer tests remain disabled. See [completion-report.md](completion-report.md) for actual build/package evidence and remaining limits. No full tablet/projector or memory-safety certification is claimed.
