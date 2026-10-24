# Claude review of PR #22: Pin settings in the Shapes and Feedback popovers

Reviewed commit: `867e2d0b899ec866b83465823f3344ac9ebd040d` (`feature/utn-m3-pinned-tool-popovers`).
Review branch: `feature/utn-claude-pr22-review`, based on that commit.

## CI of the reviewed commit

GitHub run `40071725544` finished `failure` at "Run GTK integration tests": 51 tests passed, 1 failed,
`ClassroomWorkflow.shapesAndFeedbackCanPinInspectorWithoutReselectingTools` at `ClassroomWorkflowTest.cpp:423`
(`GTK_IS_MENU_BUTTON(menu)` false). The compile error of the earlier run `40070562347` is fixed; this is a different failure.
The same test fails the same way on this machine (Debug, MSYS2 mingw64), so it is not a runner artefact.

## Root cause of the red test (test defect, not a product defect)

The test looks the menu buttons up by the accessible names set inside `createItem` ("Shapes & Lines", "Feedback Bank").
On the vertical Teacher rail `ToolMenuHandler` (around line 494) replaces those names with the rail caption
(`utn::setAccessibleName(it.get(), label)`), so the buttons are named "Shapes" and "Feedback". A widget-tree dump of
`tbLeft1` showed `GtkMenuButton name='Shapes'` and `name='Feedback'`. Lookup by the caption finds both buttons and the
author's own assertions then pass (tool unchanged, workspace 0, inspector parent, stored preference).

The names set in `createItem` still apply where `ToolMenuHandler` does not override them (horizontal toolbars), so they are not dead code.

## Reading of the production change (no defect found)

* `appendPinSettingsButton` reuses `appendMenuButton`, so the popover is closed first (`gtk_popover_popdown` is connected before the
  new handler), then `setTeacherWorkspace(0)` runs. It never calls `selectTool`, so pen, highlighter, eraser, colour and drawing type are untouched.
* The handler captures the `Control*`. The button lives inside the popover that belongs to the toolbar item; widgets are destroyed with the
  window that `Control` owns, so the callback cannot outlive `Control`. `getWindow()` is null checked. No new ownership, no new reference.
* `setTeacherWorkspace(0)` stores `workspaceTools=0` (the preference persists) and re-attaches the inspector at the saved `inspectorWidth`.
* Mode 2 (canvas only) hides the rail, so the button cannot be reached there; mode 1 is the case it serves.

## Regression coverage added by this review

`pinSettingsKeepsToolColourModeAndInspectorWidthAndClosesThePopover` (Shapes and Feedback): the Highlighter stays selected, the
drawing type and colour are unchanged, the saved inspector width is unchanged, the popover is closed, the Pin button is focusable,
sensitive and has the accessible name "Pin settings panel".
`pinSettingsIsHarmlessWhenAlreadyPinnedAndAbsentFromOtherLayouts`: clicking twice while already pinned changes nothing, and the "Xournal++"
layout has no Shapes or Feedback menu.

## Findings

| Severity | Finding | State |
| --- | --- | --- |
| P1 (blocks CI) | PR test looks up wrong accessible names; Windows CI red | Fixed in the review branch (test only) |
| P2 | The Pin entry is shown and clickable when the inspector is already pinned (mode 0); it does nothing. Hiding or relabelling it ("Settings panel is pinned", insensitive) would be clearer | Open, design decision for the owner of the popover |
| P2 | Clicking Pin writes `workspaceTools=0` even if a custom, non-Teacher toolbar contains these tools; the stored preference of a Teacher layout would be reset | Read only, not reproduced; Teacher, Tablet and Marking ship these tools, the Xournal++ layout does not. A one-line guard on `isTeacherLayout` would close it |
| P3 | Separators and the new row are not covered by a visual check | Not verified |

## Not verified

* Real key events (Tab, Enter, Space, Escape). The tests call `gtk_button_clicked`, which is the signal a focused `GtkButton` emits for Enter and Space.
  `gtk_widget_activate` was tried first and abandoned: GTK delays the click with a timer, and the result depended on test order, so it was not kept.
* The inspector pane position itself (the test reads the stored width), focus return to the menu button after the popover closes, high-contrast contrast ratios,
  Orca/NVDA output, and any native-window rendering. No native GUI control or screenshots were used.

## Skills used in this review

* `awesome-accessibility-audit` was invoked. It is written for web markup, so only its principles were applied (programmatic name, keyboard reachability,
  focus, target behaviour, dismissal); no WCAG conformance is claimed for a GTK popover.
* No other skill was invoked for this review. The root cause was found by dumping the widget tree and reading `ToolMenuHandler`; the new tests
  passed on first run against the unchanged production code, so they are characterisation tests, not red-first tests.

## Follow-up: CI run 40072892137 and the popover dismissal assertion

The new test failed on Windows CI at `gtk_widget_get_visible(popover) == true` right after Pin settings was clicked; every other assertion (workspace 0, tool,
colour, drawing type, width) passed. Root cause: the test measured a transitional state. GTK3 popovers have `transitions-enabled` on (and
`gtk-enable-animations` is on); `gtk_popover_popdown` starts a fade-out and `visible` and `mapped` both stay true until the frame-clock transition ends.
Sampled on this machine after clicking: visible and mapped true at 0, 86, 118, 147 and 175 ms, both false at 214 ms; in a second round both false at 75 ms.
So the popover was being dismissed, and the fixed 60 ms `settle()` raced the animation, which is why it passed locally and failed on the slower runner.
Fix: `waitUntilHidden()` pumps the main loop until the popover is neither visible nor mapped, with a 3 s ceiling, and the test fails with a clear message if
it never closes. There is no fixed sleep, the transition setting is left as shipped, and no assertion was removed. Local: 54/54 three times, the two pin tests five times.
