# M3 Text and Answer Box contextual settings

## Change

- Text and Answer Box rail items now offer anchored settings on repeat activation and Return/Space. Initial activation continues to select the requested tool mode.
- The floating panel is built from `ToolUtnContextBar::appendTextControls()`, so font family and size, B/I/U/S, colour, alignment, spacing, list actions, Answer Box presets, fill/border controls, answer visibility and explicit deletion continue to use the existing control/editor handlers.
- The Text/Answer Box selector rebuilds the panel after the GTK signal returns. Selecting a mode uses `Control::selectTextMode`; opening the panel itself does not alter tool selection or document state.
- No files owned by PR #22 were changed. `UtnWidgets::bindActiveToolOptions()` is reused as an existing shared helper; no shared helper implementation changes were needed.

## Verification

Run locally from `feature/utn-m3-text-contextual-settings` with the MSYS2 MinGW64 toolchain and the previously cached CMake dependencies:

- `test-gtk-integration.exe --gtest_filter=TextContextualSettings.*`: 1 passed. Covers first activation, Return repeat activation, font size and bold setting changes, colour selection, accessible controls, mode changes and Answer Box Delete/Box style availability.
- Full `test-gtk-integration.exe`: 52 passed.
- Full `test-units.exe`: 354 passed. This includes the existing list-continuation, text-reliability, lesson round-trip and undo-related unit coverage.
- `git diff --check`: clean.

The focused test drives GTK controls synthetically. Physical Windows IME composition and stylus input were not exercised. The stylus-freeze root cause remains unresolved as tracked in M3 status. The final hosted Windows CI result is recorded in the PR after it completes.

## Review notes

The popover uses the existing UTN popover panel and theme-aware GTK controls. It does not change PDF/page rendering, answer-layer classification or hidden-answer visibility while opening. Save/reopen and list behavior rely on the established editor/model paths; this batch does not modify those serialization or key-handling paths.
