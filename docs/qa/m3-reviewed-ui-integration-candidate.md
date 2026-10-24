# Reviewed M3 UI integration candidate

## Base and dependency order

- M3 base verified from the remote integration ref: `0a82a0ffbe81fdb7b216f7126ab5365e7d9e2cfa`.
- PR #23 (`c1cb9f746fdf950d2f423775b514cfe4f3d459d7`) was merged into the PR #22 feature branch. Merge result: `600bf1ba135aa328961f212b3147378878869394`.
- PR #25 (`67589d5759410e494824739864fefdf06d42e233`) was merged into the PR #24 feature branch. Merge result: `698220869a512c38bf10535a9cbb54a6e4395a20`.
- Both resulting feature branches remain available. PR #22 and #24 remain open drafts. The M3 integration branch has not moved.

## Combined changes and overlap

The separate candidate overlays both resulting feature diffs onto the verified M3 base. The only shared changed source file is `test/gtk_tests/toolbar/ClassroomWorkflowTest.cpp`. Applying the Pin branch diff and then the Text/Answer Box branch diff was clean (`git apply --check` succeeded for each); the two changes occupy separate test sections. All Pin settings and Text/Answer Box regression tests are retained. No conflict required dropping or weakening a test.

Production changes include the Shapes/Feedback Pin settings action and its unchanged-tool behavior, plus Text/Answer Box popover refresh, scrolling for short windows, and a temporary-builder listener opt-out. The Text popover reuses the existing formatting/editor handlers.

## Privacy observation and recommendation

The Answer Box popover still has a one-click “Reveal on this page” action. This candidate does not change that reviewed behavior. Safer follow-up proposal: make reveal a two-step, page-scoped action. The first activation should show the exact page and answer-layer scope to be revealed; a separately labeled confirmation then reveals only that page, while Cancel/Escape leave every answer hidden. Keep “Hide answers” immediately available. The reveal path must never change audience classification. This is an open privacy design decision for review before integration.

## Keyboard focus observation

Mode changes rebuild the mode-specific panel asynchronously and cause the old mode's toolbar item to become inactive, which closes its popover. The focused selector is destroyed during the rebuild. The focused GTK regression test in this candidate checks that closure returns focus to a visible window control, outside the hidden popover; it does not claim physical keyboard or screen-reader behavior.

## Remaining validation limits

Windows IME composition, screen-reader output, high-contrast contrast, and hardware stylus behavior have not been verified. The stylus-freeze root cause remains a release blocker. Synthetic GTK tests do not resolve it.

Hosted Windows verification runs on the candidate PR head; use its linked Actions run for the authoritative combined counts and package/portable results.
