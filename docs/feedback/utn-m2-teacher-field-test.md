# UTN M2 — Teacher field-test feedback and acceptance plan

Status: **triaged; implementation not yet started**.
Base: reviewed visual build `f9535300210ce36e1dbf04cf46d64bf52a75755b`.
Source: hands-on UTN testing by the project owner and an older teaching colleague, 23 October 2026. Screenshots from the session are user feedback, not acceptance evidence for future builds.

## Product goal

Make ordinary classroom interactions obvious on first use. Preserve working ink, selection, Answer Boxes/Layers, Student View, XOPP save/reopen, light/dark/high-contrast themes, and the resizable/collapsible inspector.

This M2 scope is the **teacher/product feedback pass**. Maintain a clear distinction between changes that are tested, physically verified, partial, blocked, and planned.

## Issue register

| ID | Priority | Teacher observation | Target behaviour | Verification |
| --- | --- | --- | --- | --- |
| M2-01 | P0 | Spotlight/Curtain stayed active; another experienced teacher struggled to exit without finding “Show the whole page.” | Clicking the active mode again toggles it off; Escape exits temporary overlay; prominent “Exit spotlight/curtain” action; current mode visibly selected; ordinary tool switches leave a safe page. | GTK activation/exit/switch tests and a fresh-teacher no-instructions walkthrough; saved annotation unaffected. |
| M2-02 | P1 | Ink/Object Eraser did not reliably erase highlighter strokes; switching modes is frustrating. | Universal, intuitive *default* erase for regular pen, highlighter and shapes, with predictable partial/whole-element semantics; explicitly separate deliberate deletion of text/images/Answer Boxes. Advanced modes remain available. | Pen+highlighter overlaps, various transparencies/thickness/zooms, masks, undo/redo, save/reopen and privacy regression tests. |
| M2-03 | P1 | Smart Shape recognition fails or transforms many hand-drawn examples incorrectly. | Measure recognition of imperfect square, rectangle, circle, ellipse, triangle, line, arrow; prefer preserving freehand when uncertain; work across zoom/stylus speed. | Corpus of real anonymised stroke traces and deterministic recognizer tests with false-positive/false-negative metrics; manual stylus pass. |
| M2-04 | P1 | Tiny chevron/arrow beside a tool icon makes mode switching hard to discover. | Tap a tool once to select; tap its already-active icon to open mode popover anchored to it; chosen mode persists until changed; keyboard/touch parity. | Test select/same-icon/popover/mode persistence/tool switch and no stale popup; ensure label/shortcut accessibility. |
| M2-05 | P2 | Need icons **with small labels underneath**, not an overly wide rail. | Balanced icon+small text beneath, consistent sizing and selected state; Automatic/Show/Hide preferences; icon-only fallback for small windows. | Real 1024x768/1280x720/1920x1080 checks and keyboard/touch access. |
| M2-06 | P1 | “Classroom” option feels isolated from the normal File/Edit/View/Navigation/Journal/Tools/Plugins/Help menu flow. | Discoverable top-level **Classroom** menu beside existing menus, exposing existing actions for teaching, marking, presentation, answer layers, worksheets and reading; quick-access controls remain. | Verify action wiring, menu placement, keyboard activation, absence of duplicates and Classic layout compatibility. |
| M2-07 | P1 | “Drag the divider to resize settings” tooltip repeatedly obscures work. | Remove pane-wide tooltip; optional concise help only on divider grip/help control; do not change resizing or persistence. | Hover/focus and actual divider dragging; no tooltip appears over the worksheet. |
| M2-08 | P1 | The Enter key **looks/acts weird** in text boxes, exact behaviour unresolved. | First reproduce with normal text and Answer Boxes; establish whether complaint concerns icon/visual affordance, Enter newline, list continuation or formatting. Make Enter/Shift+Enter intuitive without breaking IME or paragraph formatting. | Document exact repro, then test multiline, bullet/number continuation/termination, undo/redo, Unicode, XOPP reload. |
| M2-09 | P2 | Handwriting-to-font is not yet available. | Select handwritten ink, choose Convert to Text, preview/correct editable result, confirm replacement, allow undo; offline-first where supported. | Platform feasibility (API, language, licensing, local packaging), then genuine running prototype and end-to-end test; **do not label implemented if research-only**. |
| M2-10 | P2 | Overall product remains visually and interactionally unfinished. | Consolidate tool hierarchy, spacing, inspector settings and contextual affordances, guided by actual teacher screenshots rather than generic redesign. | Before/after GTK screenshots plus manual teacher walk-through; no unrelated UI rewrite. |

## Separate existing backlog, not automatic M2 commitments

Previously requested: read-aloud, dictionary, OCR for scanned worksheets, image/media insertion, PDF page management, marking stamps, colours, text autocorrection, icon/branding. Inventory what is **implemented / partial / missing** before prioritising further work. The working product name stays *Ultimate Teacher Notepad* until naming research/decision is complete.

## Delivery order and gates

1. **Classroom recoverability**: M2-01 and M2-07. Commit and test; first-time user must be able to exit transient screen effects.
2. **Ink correctness**: M2-02 and M2-03. Reproduce reported failures, add regression cases and verify mixed-annotation round trips.
3. **Interaction redesign**: M2-04, M2-05 and M2-06, preserving active tool and touch accessibility.
4. **Text and handwriting**: M2-08 reproduction/fix; M2-09 supported offline feasibility and implementation as possible without claiming speculative functionality.
5. **Teacher acceptance candidate**: review, complete tests, portable Windows package, new physical teacher feedback pass.

Every entry must conclude with one of: **Implemented + automated tested / Implemented + needs manual verification / Partially implemented / Blocked / Deferred**. Include changed files, red/green regression evidence, local/CI results and commit IDs.

## Classroom acceptance walkthrough

- Open a mixed PDF lesson with handwriting and highlighter annotations. Activate Spotlight, toggle off with the same control, activate Curtain, dismiss with Escape, and switch to Pen. No panic or mystery escape route.
- Draw a highlighter across a pen stroke and test routine erasing at multiple stroke widths, with overlaps, undo and save/reopen. The highlighter must not become mysteriously immune.
- Sketch recognisable shapes quickly and imperfectly and observe whether Smart Shape helps without rewriting handwriting.
- Select a tool, tap it again for its options, choose a mode, continue annotating, switch away and back. Mode choice persists until deliberately changed.
- At laptop and projector sizes, examine icon+label clarity, inspector size, and menu location. Classroom should be findable at the top with other application menus.
- Hover the inspector divider while annotating: no oversized instructional tooltip across the worksheet.
- Create and edit multiline normal text and an Answer Box; check Enter and list formatting before/after save/reopen.
- If a handwriting-conversion implementation is delivered, try selecting real strokes, correcting the preview and undoing the conversion. Otherwise mark as missing, not completed.

## Engineering boundaries

Development branch: `feature/utn-m2-classroom-feedback` from reviewed visual candidate. Do not merge to stable/reliability or overwrite PR #19. All changes must be independently reviewable and pass the existing regression suite plus targeted new tests; preserve previous Student View privacy and XOPP persistence guarantees.

## Implementation status (updated 23 October 2026, branch `feature/utn-m2-classroom-feedback`)

Test totals at `8008bc709`: ctest 357/357 (unit + GTK integration), Debug build, MSYS2 mingw64.

| ID | Status | Evidence |
| --- | --- | --- |
| M2-01 | Implemented + automated tested; **needs manual fresh-teacher walkthrough** | `61c70dfd9` (569f755eb did not compile), `3676b474d` (any tool switch clears the overlay; it swallowed page input), `93a3ee6f7` (on-screen "Spotlight/Curtain is ON" banner with "Show the whole page (Esc)"; one exit path for Escape, banner, Present menu). Unit `PresentationOverlay.*`, GTK `spotlightAndCurtainAreNamedOnScreenAndEveryExitWorks`. The Present menu items themselves are not highlighted when active; the banner is the indication. |
| M2-07 | Implemented (tooltip removed in `e0ccc7422`); needs manual hover check | Divider resize behaviour covered by the existing inspector GTK test. |
| M2-02 | Investigated; no hit-test defect found; safety guard + clearer names added; **needs manual stylus check** | `695531953`: 1,728-case pixel-oracle matrix for the default eraser (pen/highlighter, 6 shapes, 4 widths, 3 caps, pressure, 3 radii, click/edge/drag) found no ink left in the disc and no over-erasure; overlap + undo/redo pixel-exact; Stroke eraser deletes any touched ink. The "immune highlighter" is most likely the **Object eraser mode, which by design leaves ink alone** (or an erase on a different layer than the highlighter). `679182cf8`: the Object eraser no longer deletes page-sized pictures by sweeping, modes renamed "Eraser (pen, highlighter, shapes)", "Whole stroke", "Delete object". **Not changed:** the eraser only acts on the selected layer; a single universal mode that also deletes text/images was deliberately not built (accidental-deletion risk). |
| M2-03 | Partially implemented; **synthetic corpus only, no real traces yet** | `57fa4b3e5`, `21415845a`, `8008bc709`. Synthetic corpus (432 shapes + 936 handwriting doodles). Before: 16/108 squares became circles, recall rectangle 43, square 52, triangle 48, line 74. After: 0 wrong-class, 0 handwriting modified, rectangle 72, square 87, triangle 82, line 74, circle 108 (of 108). Ellipses (5/108) and sloppy/small shapes are still left as freehand on purpose. Outstanding: a stylus pass and anonymised real traces in `test/files/shape_corpus/` to confirm the model. |
| M2-04, M2-05, M2-06, M2-08, M2-09, M2-10 | Not started | |
