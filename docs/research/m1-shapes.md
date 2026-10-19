# M1 shapes and selection reliability

## Reproduction and cause

The pre-change `build-qa/test/test-units.exe --gtest_also_run_disabled_tests --gtest_filter=*CircleRecognizer*` ran 11 tests: nine passed and both disabled square regressions failed. Both failures returned a fitted circle for a rough square.

`ShapeRecognizer` first attempts polygons; imperfect outlines can fail that segmentation and reach the circle fallback. A symmetric square has the same isotropic inertia determinant as a circle. Its mean radial deviation also overlaps the circle acceptance threshold (roughly 0.09 against 0.10). The small-radius jitter allowance further widens the overlap. Tightening that threshold would sacrifice the intended hand-drawn circle support.

## Regression scope

The fix compares the candidate against a centered square outline over 36 orientations spanning a quarter turn. Both residuals are weighted by segment length; the square must fit at less than half the raw circle residual to override circle acceptance. The comparison runs only after the original circle gates pass. The jitter allowance and circle/polygon thresholds remain unchanged. This rejects a rough square when polygon segmentation cannot produce a rectangle; it does not promise to repair the polygon segmentation itself.

The two existing regressions are enabled. Additional direct recognition cases cover rough squares at three sizes and three rotations. Existing tests cover small and large circles, uneven sampling, slightly open loops, an ellipse, a box, and near-straight lines.

The recognized-shape lifecycle test checks every point after movement and nonuniform resize, inverse transforms, and real `DeleteUndoAction` undo/redo/undo with object identity preservation. This proves model geometry and deletion ownership. It does not exercise selection handle hit testing, GUI command dispatch, or `MoveUndoAction`/`ScaleUndoAction`; those require an interactive smoke test.

## Verification

The coordinating agent rebuilt the new tests before the production edit: `build-m1-logs/shapes-red.log` records three failing tests (the original two plus the direct rotated-square regression), with the recognized-shape lifecycle test passing.

After the production fix, `build-m1-logs/text-red-shapes-green.log` records all 13 CircleRecognizer tests passing. That combined run intentionally included the text agent's RED test, `TextReliability.UndoRedoPreservesTextStackingAndFormatting`, which failed; it is not a full-suite green result. `git diff --check` passed. The coordinating agent records final full-suite and interactive smoke-test results separately.
