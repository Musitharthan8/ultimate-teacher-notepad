# Remote and branch verification

Verified 2026-10-19 using successful `git fetch origin --prune` and authenticated `gh pr list/view`. Local HEAD and origin recovery ref both resolve to `f802b7b28ab745a5f82885214cc34be64d28f9ec`.

Current development base is `feature/utn-classroom-ui-recovery`, PR #19, open and draft, targeting `feature/utn-circular-eraser`. Latest check **Build UTN on Windows x86_64** succeeded on 2026-10-17 (run 39021830380). This is remote evidence for the base, not a fresh test of the new changes.

Dedicated working branch: `feature/utn-research-reliability`. The pre-existing untracked `install/` is preserved. No PR created, no push or merge performed.

## Open pull requests

| PR | Head branch | Base branch | Status |
|---|---|---|---|
| [#19](https://github.com/Musitharthan8/ultimate-teacher-notepad/pull/19) | `feature/utn-classroom-ui-recovery` | `feature/utn-circular-eraser` | Draft |
| [#18](https://github.com/Musitharthan8/ultimate-teacher-notepad/pull/18) | `feature/utn-circular-eraser` | `feature/utn-m1-teach` | Draft |
| [#17](https://github.com/Musitharthan8/ultimate-teacher-notepad/pull/17) | `feature/utn-m1-teach` | `feature/utn-interface-recovery` | Draft |
| [#16](https://github.com/Musitharthan8/ultimate-teacher-notepad/pull/16) | `feature/utn-interface-recovery` | `feature/utn-shell` | Draft |
| [#15](https://github.com/Musitharthan8/ultimate-teacher-notepad/pull/15) | `feature/utn-shell` | `utn-dev` | Draft |
| [#14](https://github.com/Musitharthan8/ultimate-teacher-notepad/pull/14) | `feature/student-view-preview` | `utn-dev` | Draft |
| [#13](https://github.com/Musitharthan8/ultimate-teacher-notepad/pull/13) | `feature/custom-stamps` | `utn-dev` | Draft |
| [#12](https://github.com/Musitharthan8/ultimate-teacher-notepad/pull/12) | `feature/smart-highlighter-modes` | `utn-dev` | Draft |
| [#11](https://github.com/Musitharthan8/ultimate-teacher-notepad/pull/11) | `feature/eraser-shortcuts` | `utn-dev` | Draft |
| [#10](https://github.com/Musitharthan8/ultimate-teacher-notepad/pull/10) | `feature/presentation-tools` | `utn-dev` | Draft |
| [#9](https://github.com/Musitharthan8/ultimate-teacher-notepad/pull/9) | `feature/student-view` | `utn-dev` | Draft |
| [#8](https://github.com/Musitharthan8/ultimate-teacher-notepad/pull/8) | `feature/eraser-shapes` | `utn-dev` | Draft |
| [#7](https://github.com/Musitharthan8/ultimate-teacher-notepad/pull/7) | `feature/highlighter-eraser-fix` | `utn-dev` | Draft |
| [#6](https://github.com/Musitharthan8/ultimate-teacher-notepad/pull/6) | `feature/page-labels` | `utn-dev` | Draft |
| [#5](https://github.com/Musitharthan8/ultimate-teacher-notepad/pull/5) | `feature/tool-profiles` | `utn-dev` | Draft |
| [#4](https://github.com/Musitharthan8/ultimate-teacher-notepad/pull/4) | `feature/prepare-reveal` | `utn-dev` | Draft |
| [#3](https://github.com/Musitharthan8/ultimate-teacher-notepad/pull/3) | `feature/teacher-stamps` | `utn-dev` | Draft |
| [#2](https://github.com/Musitharthan8/ultimate-teacher-notepad/pull/2) | `feature/answer-boxes` | `utn-dev` | Draft |
| [#1](https://github.com/Musitharthan8/ultimate-teacher-notepad/pull/1) | `feature/smart-highlighter` | `utn-dev` | Draft |

## Branch inventory

Only the recovery branch existed locally at session start. The dedicated reliability branch was created from its fetched remote head. Fetched origin branches:

```text
origin 9882ffaaf
origin/feature/answer-boxes f4ed2927c
origin/feature/custom-stamps 93334fa60
origin/feature/eraser-controls be6d44f21
origin/feature/eraser-shapes ed08e87a1
origin/feature/eraser-shortcuts 56917a854
origin/feature/highlighter-eraser-fix 476679eca
origin/feature/page-labels 247aef8b8
origin/feature/prepare-reveal 6a21ba4ed
origin/feature/presentation-tools dfb834870
origin/feature/smart-highlighter 690e770a3
origin/feature/smart-highlighter-modes 827523531
origin/feature/student-view d440fe9fc
origin/feature/student-view-preview f32379a6d
origin/feature/teacher-stamps 076d095ee
origin/feature/tool-profiles 5f9a0976d
origin/feature/utn-circular-eraser ed1ff4c79
origin/feature/utn-classroom-ui-recovery f802b7b28
origin/feature/utn-interface-recovery e18145312
origin/feature/utn-m1-teach 9126991d9
origin/feature/utn-shell 1ea183dc1
origin/master 9882ffaaf
origin/utn-dev de48ddd43
```

## Latest PR19 commits

```text
f802b7b28 Let the tool rail scroll on short windows instead of hiding tools in an overflow menu
c870eeb8c Show answer visibility and explain Delete Box in the Answer Box panel
f60a2ae66 Draw shapes and stroke weights on the properties panel buttons
3330c24e1 Include algorithm and vector in the tool order test
ef2420aec Align the Highlighter arrow and keep one tool order in Mark
4e828f400 Show each tool's keyboard shortcut in its tooltip
ad48de5a0 Name the UTN shell colours and widen split-tool arrows to 24 px
5800d9c7c Narrow the tool properties panel and give More colours its own row
```

PR19 also contains earlier object eraser/history ordering/hidden layer protections, teacher tools and interface fixes; see the current-state audit. Stacked PRs #15–#19 form the current classroom development line; the older feature branches are not interchangeable integration bases.
