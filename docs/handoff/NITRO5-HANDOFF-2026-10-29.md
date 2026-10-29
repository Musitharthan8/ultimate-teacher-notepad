# Nitro5 laptop handoff (2026-10-29)

Audit of all UTN work on the laptop (Nitro5) against `origin` after `git fetch --prune`.
No merges, rebases, force-pushes or application-code changes were made. Backup branches only.

## Backup branches (new, laptop-only commits)

| Backup branch | Commit | Content |
|---|---|---|
| `backup/nitro5-m3-2026-10-29` | `2275d7857` | M3 lock diagnostics + student compatibility exports. Tree identical to `origin/feature/utn-m3-integrated-classroom` (`0a82a0ffb`); only the SHA differs. |
| `backup/nitro5-m3-reviewed-ui-2026-10-29` | `4d27b85b6` | Reviewed M3 classroom UI candidate + text-mode focus fix. Differs from `origin/feature/utn-m3-reviewed-ui-candidate` (`2ef6b1da0`) by one doc file. |
| `backup/nitro5-m3-text-contextual-2026-10-29` | `5fec4c7e6` | Contextual text and answer-box settings + test. Remote `origin/feature/utn-m3-text-contextual-settings` (`698220869`) is ahead; this is an older line. |
| `backup/nitro5-m3-review-wip-2026-10-29` | `26360c07c` | Uncommitted edits from the `build-m3-review` worktree on top of `db4170ab2` (7 files, +133/-3): classroom rail labels, smart highlighter, `UtnWidgets`, `ToolHandler`, GTK test. WIP, not built as a unit. |
| `backup/nitro5-demo-stability-2026-10-29` | `f129ab69a` | Oct demo stability handoff doc, stroke-volume stress test, private-content-on-presentation-exit test. |
| `backup/nitro5-security-xopp-2026-10-29` | `6dcc153f6` | XOPP attachment hardening: bounded zip loading, libzip GHSA-m3wq-j48p-jxwh guard, shared read cache. 6 commits. |
| `backup/nitro5-diag-stability-baseline-2026-10-29` | `d7d3893d8` | 2 commits ahead of the remote baseline-instrumented branch (measurement-only diagnostics). |
| `backup/nitro5-diag-hang-2026-10-29` | `bb86569cb` | 2026-10-23 classroom hang findings doc. |

## Already preserved on GitHub (not duplicated)

`feature/utn-visual-polish` (`2702e94c8`), `feature/utn-claude-stability-oct31` (`099a95b1b`), `feature/utn-laptop-recovery-20261029` (same commit as stability-oct31), `feature/utn-m1-classroom-qa`, `feature/utn-m2-classroom-feedback`, `feature/utn-classroom-ui-recovery`, `feature/utn-claude-m3-review`, `feature/utn-claude-pr22-review`, `feature/utn-claude-pr24-review`, `feature/utn-research-reliability` (laptop is behind remote).
Detached review worktrees (`d21573c1c`, `db4170ab2`, `3ed0e6ea4`, `889234f09`, `c3e05ad61`, `1bb8e87e6`) are all reachable from remote refs.

## Not preserved (intentionally)

- `install/` in the main checkout: generated build output.
- `non-existing-file-path`: empty junk files in several worktrees.
- No stashes exist.

## Recommended integration order on the home PC

1. `git fetch origin`. Start from the home-PC paper-and-ink branch; keep it as the base.
2. Merge `origin/feature/utn-m3-integrated-classroom` (equivalent to `backup/nitro5-m3`) for lock diagnostics and student exports.
3. Merge `origin/feature/utn-m3-text-contextual-settings` (newer than the Nitro5 text backup), then diff against `backup/nitro5-m3-text-contextual` for anything missing.
4. Take the `backup/nitro5-m3-reviewed-ui` doc delta, then `origin/feature/utn-visual-polish` for the visual baseline.
5. Cherry-pick from `backup/nitro5-m3-review-wip`; expect conflicts with the redesign in `UtnWidgets` and toolbar files, so resolve against the new design.
6. `feature/utn-claude-stability-oct31` and `backup/nitro5-demo-stability`: stability fixes and tests. Rerun the soak test after merging.
7. `backup/nitro5-security-xopp`: independent; merge any time.
8. Diagnostics backups are reference only; do not merge.

Then focus on the full visual redesign and teacher usability.
