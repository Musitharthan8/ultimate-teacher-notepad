---
name: ultimate-teacher-notepad-patterns
description: "Use when working in ultimate-teacher-notepad (UTN, Xournal++ fork), especially before editing GUI toolbar code or ui/xournalpp.css, placing GTK or unit tests, naming branches, or writing commits — conventions measured from git history"
metadata:
  version: "1.0.0"
  source: local-git-analysis
  analyzed_commits: "200"
---

# Ultimate Teacher Notepad Patterns

## Commit Conventions
- Conventional Commits with scope: `type(scope): lowercase summary`. 88 of the last 200 commits follow it; the rest are plain imperative sentences (e.g. "Keep the previous file and any older backup until a Save has succeeded").
- Type mix (200 commits): fix 38, feat 17, docs 17, test 11, ui 5, ci 1, build 1.
- Common scopes: `ui`, `appbar`, `rail`, `a11y`, `eraser`, `save`, `autosave`, `student-view`, `perf`, `m3`, `qa`, `design`, `gtk`.
- Summaries state the user-visible behavior, not the file touched.
- `docs(qa)` / `docs(design)` commits record screenshots, measurements, and handoffs; they follow the code commit they describe.

## Code Architecture
- C++ with GTK. Core code under `src/core/`; GUI under `src/core/gui/`, toolbar widgets under `src/core/gui/toolbarMenubar/` (`ToolUtnContextBar`, `ToolStudentView`, `ToolMenuHandler`, `UtnWidgets`).
- Styling lives in `ui/xournalpp.css`; default toolbar layout in `resources-templates/toolbar.ini.in`.
- Save and erase paths: `src/core/control/xojfile/SaveHandler.*`, `src/core/control/tools/EraseHandler.*`, `src/core/control/Control.cpp`.
- Windows packaging: `windows-setup/package-utn.sh`.
- Hot files (200 commits): `test/gtk_tests/toolbar/ClassroomWorkflowTest.cpp` (37), `ui/xournalpp.css` (24), `MainWindow.cpp` (23), `ToolUtnContextBar.cpp` (22), `EraseHandlerTest.cpp` (20), `EraseHandler.cpp` (19).
- `core.autocrlf` is `true` locally; keep line endings consistent.

## Workflows
- UI change: edit `ToolUtnContextBar.*` / `MainWindow.*` + `ui/xournalpp.css` together, then extend `ClassroomWorkflowTest.cpp`, then add a `docs(...)` commit with real screenshots.
- Eraser or save fix: change `EraseHandler.cpp` / `SaveHandler.cpp` and update `EraseHandlerTest.cpp` in the same change.
- Toolbar changes touch `ToolMenuHandler.cpp`, `ToolButton.cpp`, `ComboToolButton.cpp` as a group.
- Branches: `feature/utn-<topic>`, `design/utn-<topic>-<date>`, `diagnostics/utn-<topic>`, `diag/<host>-<topic>`, `backup/<host>-<topic>-<YYYY-MM-DD>`.

## Testing Patterns
- GTK tests: `test/gtk_tests/` (toolbar workflow in `test/gtk_tests/toolbar/`); they run against a throw-away profile.
- Unit tests: `test/unit_tests/<area>/<Class>Test.cpp` (e.g. `control/EraseHandlerTest.cpp`, `gui/UtnContextLayoutTest.cpp`).
- Also present: `test/lock_audit`, `test/benchmarks`; soak tests queue an autosave only when none is pending.
- Name tests by behavior; verify visually on screen in default dark theme plus light, High Contrast, and constrained widths (800 px).
