---
id: utn-commit-convention
trigger: "when writing a commit message"
confidence: 0.7
domain: git
source: local-repo-analysis
---

# Use scoped Conventional Commits

## Action
Write `type(scope): lowercase summary` describing user-visible behavior. Types: fix, feat, docs, test, ui. Scopes seen: ui, appbar, rail, a11y, eraser, save, student-view, perf, qa, design.

## Evidence
- Analyzed 200 commits; 88 (44%) follow the format. Rest are plain imperative sentences.
- Mix: fix 38, feat 17, docs 17, test 11, ui 5.
