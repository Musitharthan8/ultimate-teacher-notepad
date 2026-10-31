---
id: utn-ui-change-workflow
trigger: "when changing toolbar, app bar, or rail UI"
confidence: 0.75
domain: workflow
source: local-repo-analysis
---

# Change UI code, CSS, and GTK test together

## Action
Edit `ToolUtnContextBar.*` / `MainWindow.*` with `ui/xournalpp.css`, extend `test/gtk_tests/toolbar/ClassroomWorkflowTest.cpp`, then add a `docs(...)` commit with real screenshots.

## Evidence
- ClassroomWorkflowTest.cpp changed in 37 of 200 commits, xournalpp.css in 24, MainWindow.cpp in 23.
