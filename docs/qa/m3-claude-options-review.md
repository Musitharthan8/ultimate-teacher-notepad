# Independent review: contextual-options and highlighter-preference batch

Worktree `build-m3-review`, base `db4170ab2`, uncommitted patch of 7 files (+133/−3). This is a static review only. I built nothing, ran no tests and no GUI, and edited nothing. It is not the M3 acceptance review, and nothing here should be read as GUI approval.

## Skill and tool evidence

| Item | What I did | Evidence |
|---|---|---|
| `awesome-code-review` | Invoked via the Skill tool. Followed Phases 1–5 on the 7-file diff, reading every changed file plus the callers listed below. | Skill returned "Launching skill" |
| `awesome-accessibility-audit` | Invoked. Applied its keyboard, name/role and state checklists to the new handlers. | Same |
| `awesome-test-gap-audit` | Invoked. Used its "weak test" and "absence needs all locations checked" rules on the two new GTK tests. | Same |
| `ui-ux-pro-max` | Read `SKILL.md`, then ran its local search script. Bash had no Python and `py` was missing. I used the uv-managed 3.15 interpreter that was already installed, and no `-I` flag, because the script imports its sibling `core.py`. | Two queries: `--domain ux "split button toggle popup keyboard focus return"` and `--domain icons "icon button accessible label"`. Both returned results. |

The `ui-ux-pro-max` results were generic and web-biased. The UX hit was "Focus States" (visible focus ring) and the icons hit was "decorative icon aria hidden / accessible name". Neither says anything about GTK3 split-button semantics. I used them only as a checklist and they support no finding below.

I did not invoke the test-driven-development, verification-before-completion or security skills. The diff has no security surface.

## Findings, ranked

### 1. Medium, verified source defect: the stored highlighter preference is overwritten by incidental resets, not only by user choices

The new write sits in the shared setter, `ToolHandler.cpp:405-409`, which writes `utn/smartHighlighterPreferred` on every call. Several callers use that setter for reset purposes, not as a choice:

- `Control::selectTool` (`Control.cpp:1381-1383`) calls `setSmartHighlighterEnabled(false)` whenever `type == TOOL_HIGHLIGHTER`.
  - `ActionProperties<SELECT_TOOL>::callback` (`ActionProperties.h:581-588`) reaches it for the SELECT_TOOL action.
  - So do keyboard shortcuts, the Classic toolbar's Highlighter button, stylus button config and `selectDefaultTool` (`Control.cpp:1411`).
  - `Control::initWindow` (`Control.cpp:299`) calls `selectTool(toolHandler->getToolType())` at startup. If the loaded tool is the highlighter, the preference is reset on launch.
- `Control::setToolDrawingType` (`Control.cpp:655`) calls the setter with `false` when the teacher tool policy moves a shape request from Highlight to Pen.

**Failure scenario:** the user picks "Snap to line" (smart is true, preference true). They later start the highlighter by shortcut or from the Classic toolbar, or draw a shape. The preference becomes `false`. The next click on the rail Highlight button (`ToolSmartHighlighter.cpp:54-56`) gives Freehand, even though the user never chose it. The commit exists to avoid exactly that silent mode switch. Smart is the default, so it also fails silently for most users.

**Fix:** separate the choice from the reset.
- Write the preference only from explicit choices: `Control::selectHighlighter` and the context-bar mode combo at `ToolUtnContextBar.cpp:828`.
- Leave the generic setter, and the `selectTool` and `setToolDrawingType` reset paths, side-effect free.

Related, lower severity:
- The preference is stored under top-level `utn`, but the snap mode is stored under `tools/utn/smartHighlighterMode` (`ToolHandler.cpp:733`).
- The preference is never loaded into the `ToolHandler` member. The context bar reads the member (`ToolUtnContextBar.cpp:817-818`) and the rail reads settings, so the two can disagree after restart.
- The setter doesn't call `customSettingsChanged()`. The value is written to disk only on the next save. That matches how `MainWindow` writes `utn` values, but it makes the change implicit.

### 2. Medium, verified: `bindActiveToolOptions` is applied to every `ComboToolButton`, but only to SELECT_TOOL buttons in `ToolButton`

`ToolButton.cpp:53` gates the call on `action == Action::SELECT_TOOL && toggle`. `ComboToolButton.cpp:114` binds unconditionally. `ComboToolButton` is also used by:
- the drawing-type combo (`DrawingTypeComboToolButton.cpp`, TOOL_DRAW_* actions), and
- the PDF-tool combo (`ToolPdfCombocontrol.cpp:11`).

Those buttons are not tool selectors. Pressing an active one used to do nothing (unreleasable toggle). It now opens the popover, and Enter or Space does the same.
- It may be acceptable, but the helper's header comment says "active tool", and the gate is inconsistent between the two call sites.
- No test touches the shape or PDF combos.

**Fix:** either apply the same SELECT_TOOL gate in `ComboToolButton`, or state in the helper's contract that it applies to all split toggles, and test one non-tool combo.

### 3. Medium, test gap: neither new test proves "without reselecting" or the real GTK3 event sequence

`ClassroomWorkflowTest.cpp:291` (`activeInkToolOpensOptionsWithoutReselectingAndKeepsEraserMode`):
- It drives `g_signal_emit_by_name(primary, "button-press-event", …)` (line 327) and `"key-press-event"` (line 341). Those are synthetic events with no pointer grab, no matching release and no focus. The key event has `state = 0`, `keyval = Return` and no hardware keycode.
- The press path is only half covered. The whole point of returning TRUE is that the following release must not produce `clicked` and re-fire SELECT_TOOL. The test never emits a release, so that guarantee is untested.
- The title promises no reselect, but nothing counts SELECT_TOOL activations or checks that mode state survived a press on the active tool. The eraser-mode assertions at the end use `gtk_button_clicked` and a Pen to Eraser switch. That tests `Control::selectTool`, not the new handler.
- Space, KP_Enter, the Ctrl/Alt/Super exclusion, a non-primary button, and a press on an inactive tool (should return FALSE and select normally) are all untested.
- Not tested: focus restore, Escape, a second press while the popover is open, and the combos from finding 2.

What the test does cover: the handler returns TRUE and the popover shows for four tools (Area, Pen, Eraser, Highlight). A tool switch closes the popover (`notify::active`), and that is a real check.

`ClassroomWorkflowTest.cpp:426` (`highlighterRestoresFreehandAndSemanticModeAfterSwitchingTools`):
- It is a real check of the preference round trip within a session. It is good as far as it goes.
- It never calls `selectTool(TOOL_HIGHLIGHTER)` directly or `setToolDrawingType`, so it cannot see finding 1.
- It has no persistence round trip (save and reload), and no default case (no stored key means smart).
- The test name implies durability, but it only covers one session.

For both tests, passing 321 unit and 40 GTK tests says the signals are emitted, not that a pointer or keyboard user gets the intended behaviour.

### 4. Low-medium, runtime hypotheses (unverified, GTK3 behaviour I did not run)

1. **Pointer sequence.** The press handler returns TRUE on press and opens a modal `GtkPopover`. On the following release, `GtkButton` should see no `button_down` and not click, and the popover grab should not see the release as outside. I have not confirmed either. A real press-release test is needed.
2. **Double-click.** GTK3 sends `GDK_2BUTTON_PRESS` as a separate event whose `event->button` is also 1. A fast second click may be consumed by the popover grab or may re-enter the handler. This only needs a manual or event-level check.
3. **Focus after close.** GtkPopover restores focus toward its relative widget, which is the arrow `GtkMenuButton`, not the tool button. Keyboard users may lose their place after Escape. I have not checked this.
4. **Assistive technology.** AT-SPI "click" or "press" calls `gtk_button_clicked` and bypasses the new key and press handlers. That re-fires the action, which is the old behaviour (re-selecting resets modes). The tool button also does not expose that it has a popup.
5. **Arrow button names.** The `utn-tool-arrow` menu buttons built in `ToolButton.cpp:49` and `ComboToolButton.cpp:108-110` have no `setAccessibleName`. Only the highlighter arrow has a tooltip ("Highlight mode"). That predates this diff, but the new handlers make those unnamed buttons the thing a screen reader user lands on after Enter. Suggested name: `"<Tool> options"`, in line with the `utn::setAccessibleName` usage elsewhere. This is Medium by the audit skill's scale (WCAG 4.1.2).

### 5. Callback lifetimes and programmatic sync (checked, no defect found)

- `g_signal_connect_object(tool, …, options, 0)` (`UtnWidgets.cpp:108-132`) ties the three handlers to the menu button's lifetime. The `options` pointer is only dereferenced while the handler is connected, so I found no dangling-pointer path in the GtkToolItem box layout.
- `notify::active` fires for programmatic sync, which is the intended way to close the popover on a tool switch. It does not conflict with `syncToggle`'s `utn-syncing` guard in the Highlight `clicked` handler.
- The `Control::selectTool` / `ToolHandler::selectTool` interplay (`ToolHandler.cpp:194` clears the member without touching the stored key) is consistent with finding 1. The write at line 408 is the only mutator of the stored key.

### 6. Nits

- `ToolSmartHighlighter.cpp:14` places `control/settings/Settings.h` before `control/actions/ActionDatabase.h`. This is only an include-order nit.
- The helper's key handler accepts Shift+Enter and Shift+Space. That is fine, but undocumented.

## Limitations

- No build, tests, GUI or native interaction. The 321 unit and 40 GTK passes come from your report and were not re-run.
- Items in finding 4 are hypotheses from GTK3 knowledge, not observations.
- I did not read the GTK3 source. I read the project's own `setToggleButtonUnreleasable` (`GtkUtil.cpp:41`) for the action and toggle semantics.
- I did not check `Settings::save` output to confirm `utn` custom elements are written; I relied on `MainWindow` using the same pattern.
- The `ui-ux-pro-max` search gave only generic guidance.
- The 7 changed files were read in full. Callers were read only at the cited locations.

## Verdict

Not ready to merge as it stands. Finding 1 should be fixed (or consciously accepted) before this batch is merged, because it silently undoes the feature's purpose. Finding 2 needs a decision. Finding 3 should be closed with a real press-release test and a direct `selectTool(TOOL_HIGHLIGHTER)` regression test. Native keyboard and pointer checks (finding 4) are still needed once GUI control is allowed again.
