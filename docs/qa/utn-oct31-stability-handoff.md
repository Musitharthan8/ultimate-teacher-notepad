# UTN October 31 Demo Stability Sprint Handoff

Branch: `feature/utn-demo-stability-oct31`  
Base: verified M3 candidate `2ef6b1da0669963f87772f24ed2fe2cd4fd08c65`  
Worktree: `C:\Users\musit\Documents\utn-demo-stability-oct31`

## Changes in this sprint

- `d3f3d2e44 test: preserve private content on presentation exit` adds a GTK regression test for Spotlight/Curtain dismissal, saved annotations, and hidden Answers content after save/reopen.
- `daf6e6307 test: report input point volume in stroke stress run` records stroke and input-point counts in the existing repeat-stroke GTK stress test.

No production-code fix has been made for the field stylus freeze. Existing presentation behavior already supports Escape, active-mode toggle, tool-switch dismissal and a visible “Show the whole page (Esc)” action; the new test guards its privacy and persistence behavior.

## Freeze evidence and limits

The synthetic 40,000-gesture run exercised 20,000 pen and 20,000 highlighter strokes, each with 17 input samples (340,000 samples per tool). Three instrumented repeats passed. They showed GTK heartbeat gaps of roughly 61–101 ms and maximum single-stroke timings of roughly 75–131 ms on this Windows Debug build. This establishes latency spikes under synthetic load, not the reported hardware freeze.

A five-page autosave/render/lock soak passed at 6,000 and 20,000 gestures. The 20,000 case completed in 63.796 s; maximum measured document-lock wait was 1.410 ms, with zero slow lock waits. This makes document-lock contention less likely in this test scenario, not ruled out for physical stylus input.

Source inspection found a rendering optimization lead: `StrokeHandler::onButtonReleaseEvent` inserts the stroke into the layer, paints and clears overlay views, then calls `fireElementChanged`. `XojPageView::elementChanged` compares the element against `inputHandler->getStroke()`, which is null after the stroke is moved. The “already painted” no-rerender path therefore appears unavailable for this completed-stroke path. No A/B measurement has established its cost or its relationship to the hardware freeze. Validate with a focused render-count/CPU comparison before changing ordering or invalidation behavior.

No crash report or real stylus trace was available. A synthetic test cannot establish a hardware-only fix. Native GUI launch was attempted through the approved computer-use route and blocked by its tool permission (`Computer Use was not approved to use xournalpp`); no alternate UI automation was used.

## Other focused findings

- Eraser behavior already has pixel-oracle coverage for pen/highlighter plus tests for undo/redo, partial erase, hidden layers and PDF/background protection. No reproducible defect was found to justify changing it.
- Smart Shape's deterministic corpus showed zero wrong-class conversions and preserved its handwriting negatives. In the sampled corpus, recognition recall was circles 108/108, lines 74/108, rectangles 72/108, squares 87/108 and triangles 82/108. Sloppy small shapes commonly remained freehand. The corpus varies sample spacing and geometric perturbation but does not vary pressure; arrow is a negative/freehand case, and the recognizer currently has no arrow snapping implementation. Do not interpret these synthetic rates as field accuracy.
- Source search did not locate a persistent inspector-divider tooltip. Existing tests cover classroom navigation, tool variants/popovers and Enter-key handling; no confirmed defect was isolated in those areas.

## Verification evidence

Configured in MSYS2 MinGW Debug with:

```sh
cmake -GNinja .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_GTEST=ON -DDOWNLOAD_GTEST=on -DUTN_LOCK_AUDIT=ON
```

Built with:

```sh
cmake --build build --target xournalpp test-units test-gtk-integration test-lock-audit --parallel 4
```

Baseline full suites passed: unit 356/356, GTK 62/62, lock audit 6/6. After the changes, the new presentation GTK test passed; the instrumented 40,000-gesture stress and both multi-page soak runs passed. `git diff --check` passed before commits. Existing compiler, GTK scale and lock-watchdog warnings were not suppressed.

Stress commands were run from `build`:

```sh
UTN_STRESS_STROKES=20000 CI=true ./test/test-gtk-integration --gtest_filter=ClassroomWorkflow.repeatedInkThroughPageInputRetainsHistoryAndRoundTrips --gtest_output=xml:stress-instrumented-40000.xml
UTN_SOAK_STROKES=6000 CI=true ./test/test-gtk-integration --gtest_filter=ClassroomWorkflow.longSessionWithAutosaveAndSeveralPagesKeepsTheDocumentLockHealthy
UTN_SOAK_STROKES=20000 CI=true ./test/test-gtk-integration --gtest_filter=ClassroomWorkflow.longSessionWithAutosaveAndSeveralPagesKeepsTheDocumentLockHealthy
```

## Review priorities

1. Correlate the teacher's crash report and device/driver details when supplied; reproduce on the actual stylus hardware.
2. Add a controlled before/after render-count and latency experiment around completed-stroke page rerendering. Preserve visible-buffer correctness, zoom behavior, clipping and layer stacking.
3. Expand Smart Shape corpus with sparse fast traces and varying pressure; include arrows only if recognition is intentionally implemented. Keep uncertain input as original freehand.
4. Retest all privacy, persistence, autosave and annotation workflows on Windows GUI when approved UI access is available.
