# M1 stabilisation implementation plan

Date: 2026-10-19. User scope is the M1 stabilisation directive; independent engineering choices below implement it without expanding the UI. Base `ceeeb0fc0` on `feature/utn-research-reliability`, with previous `5b9238b9a` retained. Fetched PR19 remains `f802b7b28`; no newer relevant remote changes, no merge. Existing untracked `install/`, `build/` and earlier portable ZIP are preserved. The baseline test tree was reused only for focused regression builds; new application/package output goes in `build-m1`.

## Architecture and acceptance

1. **Partial erasing:** centreline deletion cannot represent a hole smaller than a wide stroke. Retain vector stroke data and store swept circular erase capsules in document units. `StrokeErasure` has start/end, radius and an affine transform so move/rotate/nonuniform resize carry holes with the stroke. Apply subtractive Cairo clips per stroke; overlapping erased regions must remain erased. Exact capsule sweeps replace pointer-path sampling for partial mode only. Whole-stroke and Object modes retain the prior correction. Reject nonfinite pointer input.
2. **History:** reuse `EraseUndoAction` ownership of original/edited elements, snapshot a stroke once per gesture, swap at its original layer position, and record final edited positions for redo. Fully covered bounds may be removed; small holes preserve an editable stroke. Emit page change notifications after releasing the document lock.
3. **Persistence:** ordinary XOPP strokes are unchanged. Masked strokes save a bounded-resolution standard image fallback plus validated versioned text metadata for UTN's editable vector state. Older readers show the actual clipped appearance instead of revealing erased ink. Metadata malformed/unsupported/transformed by an older reader falls back to the saved image. A legacy reader resave may lose vector metadata; image appearance remains the compatibility floor. Full current-version save/reopen and PDF/PNG fidelity are required before completion. Clipboard serialization must preserve masks and remain able to read earlier unmasked strokes.
4. **Shapes:** reproduce both disabled regressions; discriminate square outlines from circular candidates without raising global recognition thresholds. Add rotated/size cases, preserve circles/ellipses and model transform/delete history. Do not claim GUI selection validation from model tests.
5. **Text:** preserve text's layer index when swapping edited/original elements in finalization and undo/redo. Test real history actions beneath overlapping foreground content and formatting/Unicode persistence. Keep existing list continuation and focus mechanisms unless a separate fault is reproduced.
6. **Audience:** trace prior recursive-lock crash and overlay click lockout; add repeated reveal/hide real-renderer privacy and save/reload checks. Explicitly distinguish Student View filtering from ordinary PDF export and deliberately frozen prior frames. Real monitor/tablet/GUI activation checks remain separate.
7. **UI:** source/evidence audit and a compact UTN proposal only. Preserve current responsive labels, scrolling rail, contextual controls, themes and tool bindings. No broad rewrite in this phase.

## Tasks and proof

- [x] Verify Git/PR state and rerun 282 enabled units and 29 GTK tests.
- [x] Reproduce original square failures and new rotated-square regression; implement bounded recognizer correction.
- [x] Reproduce text history stacking failure; implement index-preserving swap.
- [x] Reproduce wide-ink loss outside radius at zoom 1/2/4 before eraser implementation.
- [x] Implement and validate vector clips, history, transforms, intersections, small radii and fast sweeps.
- [x] Prove masked persistence, malformed metadata fallback and ordinary document compatibility.
- [x] Run complete units/GTK; explain additions and two re-enabled recognizer cases relative to 282/29.
- [ ] Build a new portable Windows package under `build-m1`; test runtime exports with system-only PATH.
- [ ] Independent diff review, fix material findings, commit bounded changes and provide manual checklist/executable paths.

Files: eraser model/Stroke, StrokeView, EraseHandler, EraseUndoAction, versioned MaskedStrokeCodec and XOPP save/load/parser interface; CircleRecognizer and tests; TextEditor/TextBoxUndoAction and tests; LayerAudience tests. No new external dependency, service or skill installation. New tests go in existing test targets after CMake regeneration.

Review focus: erased-region overlap; masked strokes transformed repeatedly; multiple elements deleted/edited in one gesture and correct z-order after undo/redo; highlighter opacity/PDF clips; malformed or missing metadata; safe raster memory bounds; persistence at fractional zoom; hidden audience content; true circles not rejected with squares. Model bounds may conservatively include erased holes, preserving selection access; this is not pixel-shaped selection. Long, repeatedly curved erase paths increase clip complexity and require profiling beyond a basic smoke test. No hardware latency claim follows from these tests.
