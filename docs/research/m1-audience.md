# M1 Answer Layer and Student View investigation

Reviewed 19 October 2026 on `feature/utn-research-reliability` (baseline `ceeeb0fc0`). This investigation reads current source and repository history and adds focused renderer regressions. It does not certify real projector or stylus behavior.

## Historical root causes

- `867e16a2a` documents and fixes the Start Answers Layer crash/deadlock: `Control::startAnswersLayer()` acquired the document's non-recursive exclusive mutex, then called `getCurrentPage()`, which acquired a shared lock on the same mutex. Current code reads `doc->getPage(getCurrentPageNo())` under the existing exclusive lock and notifies layer menus after releasing it. This is a historical cause, not a newly reproduced crash.
- `6ae73e5af` replaced name-based Student View exclusion with persisted layer audience. Previously renaming answers could expose them and Reveal failed to reach students. Current `visibleToStudents()` rejects TeacherOnly unconditionally and Answers unless visible; explicit saved audience overrides legacy name migration. Answers always become hidden on load.
- `3f7d7b00f` moved selection off answers on every page when hiding, rather than only the current page. Its historical all-answers-page gap was later addressed at the content-creation boundary by `98b88e02f`: a visible ordinary layer is selected, an ordinary hidden layer is shown, or a new layer is created. Teacher ghost editing remains intentionally possible.
- `59877e6bf` addresses a different apparent lockup: the frozen/blank teacher outline owned a GdkWindow that swallowed toolbar clicks. Current `updateTeacherOutline()` makes the drawing area windowless and marks it as a pass-through overlay. This UI input obstruction is distinct from the document mutex deadlock.

## Source findings

`ToolStudentView::renderStudentFrame()` holds a shared document lock and installs the student audience filter on a fresh `DocumentView`. The renderer's ghost fallback explicitly requires the absence of a layer filter. Thus the teacher ghost opacity cannot itself bypass the Student View filter. Student redraw events are coalesced to a 40 ms timeout; page selection/change notifications request redraw. Frozen mode stores the filtered rendered frame and blank mode paints black.

No new reproducible P0/P1 was established in this scoped source investigation; no speculative production patch was made.

## Regression additions

Added two cases to `test/unit_tests/model/LayerAudienceTest.cpp`:

- `studentRendererNeverGhostsHiddenAnswersOrTeacherNotes`: renders real stroke markers through DocumentView with the same filter as Student View and teacher ghost opacity 0.4. Twenty hide/reveal cycles assert ordinary ink stays black, hidden answers stay white, revealed answers become black, and visible teacher notes remain white. Regressing the filter handling or allowing ghost fallback for rejected layers makes the pixel assertions fail.
- `studentRendererKeepsSavedRevealedAnswersHiddenAfterReload`: saves visible answers under a nonlegacy name plus visible teacher notes, reloads through production handlers, and asserts both markers are absent from student-rendered pixels. This exercises persistence and rendering together, beyond metadata-only assertions.

The lead ran shared `build-qa` builds and reported all 18 LayerAudience cases passing, including both added pixel regressions. Existing LayerAudience and LessonRoundTrip cases also cover name independence, legacy migration, writable-layer routing helpers, audience persistence, and repeated lesson save/reopen. This result does not establish the separate GTK or hardware checks below.

## Limits and release checks

These pixel tests exercise the renderer, not GTK window creation, repeated `startAnswersLayer()` activation, actual page-navigation callbacks, timer lifecycle, live editing or monitor changes. Those require the lead's integration/manual exercise. No actual second monitor, projector, tablet, stylus or palm rejection was tested here.

Freeze intentionally preserves an earlier frame: answers already revealed when frozen remain in that frame even if later hidden in the editor. This is existing freeze semantics, and must be distinguished from hidden answers entering a fresh student render. Blank overrides the frozen frame.

Ordinary PDF export is not Student View export: `XojCairoPdfExport` uses visibility-based `DocumentView::drawPage()` without the student filter, so hidden answers are excluded by default but visible TeacherOnly notes remain exportable. Explicit layer-range export uses `drawLayersOfPage()` regardless of visibility. Therefore a claim that every export automatically strips all teacher-only content would be incorrect; test the exact intended export mode.
