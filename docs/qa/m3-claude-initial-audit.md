# UTN M3 pre-implementation audit (read-only)

**Commit:** `f4e54c4d9a4ddfc4c9a0ec026170ec3237c57eba`, detached worktree `build-m3-review`. `git status` was clean at start and I edited nothing.

**This audit is static code reading only.** I did not build, run tests, run the GUI, profile, or take screenshots. Every item below is either "confirmed by reading source" or an "unmeasured hypothesis". Nothing is observed behaviour.

## 1. Skills and MCP inventory (discovered, not taken from Codex)

- **`C:/Users/musit/.agents/skills`** has 38 entries. Relevant ones are the `awesome-*` audits, `ui-ux-pro-max`, `design-system`, `investigate-first`, `surgical-patch`, `verify-and-stop` and `reverse-engineer-anything`.
- **No `AGENTS.md` or `CLAUDE.md` in the repo.** The only applicable instruction file is the user's global `~/.claude/CLAUDE.md`, which covers graphify only.
- **Skills invoked via the Skill tool, each returned its instructions:**

| Skill | How it was applied |
| --- | --- |
| `caveman:investigate-first` | Evidence before cause. §2 separates confirmed from hypotheses; no fix proposed. |
| `awesome-performance-audit` | Measure the tail, not the average. Triage and USE method before code. The "no profile, no number" rule is why §2 labels most items as unmeasured. |
| `awesome-privacy-audit` | Used the data-flow and "store/recipient" method for §4: trace write path to each render and export sink. |

- **`ui-ux-pro-max`:** I read its `SKILL.md` directly (priority table: accessibility, touch/interaction, layout) and used it as a lens for §3. I did **not** invoke it through the Skill tool, because it is not in the Skill registry. I did not run its `search.py`.
- **Not used:** `reverse-engineer-anything`, `awesome-accessibility-audit` and Computer Use. No native GUI automation was attempted.
- **MCP:** browser-use and firecrawl are available, but the task needs neither.

## 2. Dense repeated-stroke freeze

### Confirmed by reading source

- **C1. The "skip rerender for a fresh stroke" optimisation never fires.**
  - `StrokeHandler.cpp:210` moves the stroke into the layer with `layer->addElement(std::move(stroke))`.
  - `StrokeHandler.cpp:216` then calls `page->fireElementChanged(ptr)`.
  - `PageView.cpp:1285` tests `elem == inputHandler->getStroke()`, and `getStroke()` returns `stroke.get()` (`InputHandler.cpp:25`). That is now null, so the test is false.
  - `noRerender` is therefore always false and `rerenderElement` runs for every stroke. The comment at `PageView.cpp:1277-1283` says this should not happen.
  - Whether this causes the field freeze is unmeasured.
- **C2. The undo stack is unbounded.** `undoList` is a `std::deque` with no cap (`UndoRedoHandler.h:63`, `addUndoAction` at `UndoRedoHandler.cpp:158`). Memory grows with every stroke for the whole session.
- **C3. The main thread can wait on a render worker.**
  - `StrokeHandler.cpp:209-211` and `InsertUndoAction::undo/redo` take the exclusive `doc->lock()` on the UI thread.
  - `RenderJob::renderToBuffer` holds a `std::shared_lock<Document>` for the whole `drawPage` (`RenderJob.cpp:119-120`).
  - `Document` uses a plain `std::shared_mutex` (`Document.h:176`).
  - Any in-flight full-page render therefore blocks pen-up, erase, undo and redo until it finishes. Pen-up is where the freeze would be seen.
- **C4. Autosave holds the document lock for the whole write phase.** `AutosaveJob::run` holds the shared lock from `lock_shared()` to `unlock_shared()` across `prepareSave`, and takes an exclusive `doc->lock()` for `updateDocumentInfo`. On a dense document, that exclusive lock can stall the UI thread.

### Unmeasured hypotheses, ranked by plausibility

1. **C1 + C3 together.** Each pen-up queues a render job (`XournalScheduler::addRerenderPage`). That job scans all layer elements linearly with an `intersectsArea` test (`LayerView.cpp:34-41`), and highlighter strokes composite with MULTIPLY. Page render cost therefore grows with stroke count, and the exclusive lock waits behind it. This would match "lag, then freeze".
2. **Page-wide rerenders on undo, erase and zoom.** `rerenderPage` rebuilds a full-page `Mask` (`RenderJob.cpp:72-77`).
3. **Unbounded undo memory** pushing the process into swap on a 4–8 GB laptop.
4. **Autosave (C4) landing mid-session.**
5. **Smart Shape / recognizer cost per stroke.** The recognizer runs at pen-up only when the drawing type is the recogniser (`StrokeHandler.cpp:195`). It is probably cheap.
6. **Windows pen event flood.** No motion-history or batching code was found (grep for `motion_history` returned nothing). The point filter is only a 0.3 px threshold (`InputHandler.h:104`).

### Measurement plan (objective and reproducible)

1. **Corpus.** A deterministic generator, seeded and committed, producing N strokes: 1k, 5k and 20k, in mixed pen and highlighter, with overlap, at zoom 100%, 200% and 50%. Inject them through the real `StrokeHandler` path or synthesised GDK events, not by loading an XOPP file, because loading skips the input and undo path.
2. **Metrics.** Per-stroke pen-up latency (p50/p95/p99/max), UI-thread max stall, time spent waiting for `doc->lock()`, `RenderJob` duration versus element count, and RSS and undo-list size versus N.
3. **Instrumentation.** Temporary timing wrappers around `doc->lock()` and `RenderJob::run`, plus a main-loop watchdog that logs gaps over 100 ms. Build with frame pointers, and capture a sampling profile or ETW trace of the main thread.
4. **A/B on one build** to isolate each cause:
   - Disable the C1 rerender.
   - Disable autosave.
   - Cap undo depth.
   - Release the shared lock between layers.
5. **Pass criteria,** fixed before testing: pen-up p99 under 50 ms and no UI stall over 200 ms at N=5k on the portable Windows build. Real stylus hardware stays an owner-only final item.
6. **Evidence.** Raw logs and traces go outside the repo, named by target and timestamp.

### Testable architectural alternatives (adopt only if the data supports them)

- Fix C1 by capturing the pointer before the move and comparing against it.
- Make the render job copy the element list under a short shared lock, then draw without holding the lock.
- Cap undo depth, or spill old undo actions.
- Coalesce page rerenders to once per N ms.

## 3. Active-icon popover without breaking GAction, toggle or focus

### Current mechanism (source)

- Rail buttons are `GtkToggleButton`s bound to `win.<action>` with a target value, via `ToolButton::createItem` (`ToolButton.cpp:34-41`).
- They use `setToggleButtonUnreleasable`, so clicking the already-active button keeps it checked.
- They have `focus_on_click=false`.
- The pen and eraser style popovers hang off a **separate** small `GtkMenuButton` chevron with class `utn-tool-arrow` (`ToolButton.cpp:47-58`). This is exactly the tiny chevron the teachers complained about.
- `ComboToolButton` follows the same pattern (`ComboToolButton.cpp:88-113`).

### Hazards

- **H1. Re-activating `SELECT_TOOL` has side effects.**
  - The callback (`ActionProperties.h:577-586`) always runs `clearSelection` and `ctrl->selectTool(tt)`.
  - `Control::selectTool` resets Answer Box and teacher stamp on `TOOL_TEXT`, turns off Smart Highlighter on `TOOL_HIGHLIGHTER` (`Control.cpp:1374-1385`), and fires `fireToolChanged`.
  - Whether a same-target click reaches the callback depends on GTK3's radio action handling and the unreleasable helper. That was not verified.
  - **Rule:** the popover trigger must not be a second activation of the action. Intercept the click before it reaches the action, using a GTK3 `button-press-event` handler or a `GtkGestureMultiPress` in capture phase on the button. Compare the current `SELECT_TOOL` state with the button's target. If they match, open the popover and stop the event. Otherwise let the click through unchanged.
- **H2. Do not use "double-tap".** The spec requires a keyboard and screen-reader path. Provide one of these:
  - A `GtkMenuButton` that stays focusable and visible to assistive technology, even if visually secondary.
  - Enter or Space on the focused, already-active button, handled in `key-press-event`.
  - A separate action such as `win.open-tool-options`.
  - Do not rely on the main-window shortcuts alone.
- **H3. Editor focus.**
  - Buttons already use `focus_on_click=false`, and the canvas takes focus through `XournalView::requestFocus` (`XournalView.cpp:363`).
  - A `GtkPopover` grabs focus while open. On popdown, return focus explicitly to the canvas or the text editor, otherwise Enter and typing may go to the popover. This matters for the Text and Feedback tools.
  - Escape and outside click must pop down without changing state. `GtkPopover` already does this by default.
- **H4. Persistence is separate from the popover.** Per-tool subtypes already have actions (`TOOL_PEN_LINE_STYLE`, `TOOL_ERASER_TYPE`, drawing-type actions). Selecting a popover entry should set those actions, which persist today. Do not add parallel state.
- **H5. Do not trigger page rerender, zoom or pointer changes from popover open and close** (spec item 7). Anchor the popover to the button. Avoid showing the inspector as a side effect.
- **H6. Mutual exclusion.** One open popover at a time, and close it when the tool changes. This is the "no stale popup" test from M2-04.
- **Tests to add** (GTK harness): re-click opens the popover without a `SELECT_TOOL` state change or callback call; first click selects without opening; Escape leaves state unchanged; the keyboard path opens it; focus returns to the canvas; Text-tool Answer Box mode survives re-click; the chevron, if kept, is secondary.

## 4. Student View and export privacy

### Confirmed by reading source (highest-severity finding)

- **P1. Teacher-only layers are not excluded from exports, print or preview.**
  - `DocumentView::drawPage` uses `layerFilter ? filter(layer) : layer->isVisible()` (`DocumentView.cpp:84`).
  - `setLayerFilter` is called only in Student View (`ToolStudentView.cpp:73`) and in a test. A grep over `src` and `test` found no other caller.
  - `XojCairoPdfExport.cpp:144`, `ImageExport.cpp:237`, `PrintHandler.cpp:55`, `PreviewJob.cpp:57` and `SaveJob.cpp:91` all create a default `DocumentView`.
  - A `TeacherOnly` layer is shown on the teacher canvas, so `isVisible()` is true for it. **I read this as it being drawn into a PDF/image export or print.** I did not run an export. Layers marked `Answers` start hidden on load (`prepareLoadedLayer`), so they would be excluded unless the teacher has revealed them.
  - The roadmap requires "Exclude teacher-only content from student exports independently of what is visible" (Roadmap §9). M3 requires that "hidden answers never leak to student display or student export".
  - **No distinct "student export" mode exists.** The fix is an explicit export audience, Teacher or Student or Marked, that sets the filter in each of those exporters.
  - Missing test: export a page that has a TeacherOnly layer and one Answers layer, in both revealed and hidden states, and assert on pixels. The current `LayerAudienceTest` only exercises the Student View renderer.
- **P2. The XOPP file itself carries all layers.** `SaveHandler` writes the audience attribute on every layer (`SaveHandler.cpp:197`). That is correct for a teacher file, but a "student copy" must be a separate, filtered artifact.
- **P3. The teacher canvas can ghost hidden answers.** `RenderJob.cpp:114-116` calls `setHiddenAnswersOpacity(0.4)` only when the code allows it. Per `Control.h:234` that is safe only with a separate student display. The toast in `ToolStudentView.cpp` says answers stay hidden on a single screen. I did not verify the gating condition, so this is a **needs-verification** item.
- **Gap to check when the rail and popovers are redone:** transient help, toasts or popovers must not appear over the student display, and the status bar must show the *real* Student View state.

## 5. Ranked summary

**Confirmed (by reading):**
1. P1: teacher-only layers not filtered from export, print or preview (privacy; release-blocking).
2. C3: the UI thread's exclusive `doc->lock()` waits behind render jobs. This is a confirmed lock relationship; whether it causes the freeze is unmeasured.
3. C1: the no-rerender optimisation is dead because `stroke` is moved before the comparison.
4. C2: the undo stack is unbounded.
5. H1: re-activating `SELECT_TOOL` resets Text and Highlighter modes if popover opening reuses the action.
6. C4: autosave holds locks across a long phase.

**Unmeasured hypotheses:** the §2 list ordered 1–6 (the C1 + C3 combination first). Nothing in that list is established until the measurement plan has run.

## 6. Remaining M3 gates (from the spec; none verified here)

- The skill-execution log `docs/qa/m3-skill-execution-log.md` (not checked for existence beyond the design doc requiring it).
- The performance corpus, the before and after report, and the portable Windows build test.
- A GTK test for the active-icon popover, covering keyboard, focus and toggle semantics.
- A privacy test for each export path.
- M2 items still not started: M2-04, M2-05, M2-06, M2-08, M2-09 and M2-10.
- M2-03: real-stylus traces.
- Native GUI screenshots from the compiled executable at 1024×768, 1280×720 and 1920×1080, in all three themes.
- Green Windows CI, a short owner acceptance plan, and no merge or release.

## 7. Scope and limitations

- Static reading of about 20 files in `src/core`, plus the four required docs. No build, test, GUI, profiler, screenshot or hardware run.
- I did not read the full `ToolMenuHandler`, `Eraser` or text-editor code. M2-02 eraser semantics and M2-08 Enter behaviour are therefore not audited.
- I did not check the Windows packaging and CI configuration.
- Screenshots in `docs/design/screenshots` are historical baselines, not M3 evidence, and I did not rely on them.
- Not Codex-related: I did not read the Codex execution log, and I did not use the shared main checkout.
