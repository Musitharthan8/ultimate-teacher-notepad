# Kami versus UTN feature matrix

## M1 delivery status — 20 October 2026

This table supersedes the historical implementation gaps below. Friday 23 October is the release target, not a claim of readiness. The previous workstation passed 312 unit and 29 GTK tests; the home PC has no native toolchain. CI runs `39282449185` and `39282448998` were still queued when inspected; no new C++ results are available from them.

| Area | Implemented / confirmed | Remaining gate or missing feature |
|---|---|---|
| Windows test build | Feature CI now packages a portable ZIP, samples and SHA256 manifest after units/GTK; extracted ZIP smoke checks launcher, exports and GUI message loop with system-only PATH | Artifact generation and download pending runner execution; unsigned Debug build, not final Release installer |
| Eraser precision | Swept circular subtraction, thick-ink preservation, mask persistence and history regressions committed | Physical stylus latency, zoom/transforms and smallest-size precision unverified; dashed/fill/non-round hit approximations remain |
| Shapes | Both formerly disabled square regressions enabled; rotation/size and lifecycle cases committed | Native move/resize/delete/history workflow unverified |
| Answer Layers | Hidden-on-load audience metadata, writable-layer routing and repeated render/save/reload checks | Repeated live Control activation/edit/save/page navigation unverified |
| Student View | Audience filtering, freeze/blank and windowless input overlay implemented | Actual second monitor, DPI, hotplug and timer lifecycle unverified; Freeze retains the earlier frame intentionally |
| Document editing | Text stacking fix and save/reload regression coverage committed | IME/RTL/popover focus and mixed-object GUI workflow unverified; paragraph-aware list conversion remains missing |
| PDF export | Production pixel regression retained; SOURCE/OVER comparison configured | Comparison pending; do not claim ruling loss. Ordinary exports are not universally student-filtered |
| Packaging blocker | Removed unconditional QPDF DLL copy; dependency walker includes it when linked | End-to-end portable run pending; signing, installer upgrade/uninstall and memory checks remain release gates |

Download instructions once CI succeeds: open the matching **UTN Windows Build** run, download `UTN-Windows-x64-test-<commit>`, extract the artifact and its inner Portable ZIP, then run `bin/UTN.exe`. Confirm `SOURCE-COMMIT.txt` matches the intended commit. No local MSYS2 installation is required. Test copies of the included samples before classroom documents.

## Historical comparison

2026-10-19. Kami evidence IDs and limitations are expanded in [kami-findings.md](kami-findings.md); source pointers and validation limits in [utn-current-state.md](utn-current-state.md). “Offered” means a control was inventoried, not that its functionality worked. Unknown Kami behavior is never counted as a UTN gap. Priorities are UTN classroom priorities, not feature parity scores. No demonstrated P0 issue was established by this audit.

| Feature | Kami evidence | UTN implementation | Gap / action | Priority |
|---|---|---|---|---|
| Toolbar grouping | Baseline named rail; K-UI-002 description toggle, 150/50 CSS px | Teacher icon rail, shared order, contextual panel | Existing implementation; validate latest short-window scrolling rather than replace | P1 |
| Contextual options | K-DRAW-001, K-TEXT-001, K-SHAPE-001 | `ToolUtnContextBar` controls per tool, 208px panel | Validate unclipped controls and stable fit zoom across tools | P1 |
| Icons/labels/tooltips | Named controls, pressed states; visible Move toolbar tooltip | Accessible tool identities, live accelerator tooltips, drawn shapes/weights | Native keyboard/screen-reader/high-contrast pass outstanding | P1 |
| Placement/layout | Move toolbar offered, untested; six viewport captures | Configurable toolbar presets; vertical scroller | Native tablet/DPI/short-window evidence needed; web CSS px are not native sizes | P1 |
| Themes | Dark chrome visible; switching unknown | Light/dark/high contrast CSS tokens and icons | Verify all selected/disabled/hover states on current build | P1 |
| Pen | K-DRAW-002 one mouse dot/undo; no handwriting | StrokeHandler, stabilizer, pressure-aware rendering | No comparative quality proof; device latency and trajectories needed | P1 |
| Width/colors | K-DRAW-001 width list 0.1–12 (units unknown), palette/hex/custom controls | Five weight presets, custom palette and chooser, tool profiles | Preserve settings and custom options; no reason to copy numeric web units | P2 |
| Pressure/zoom | No tested pressure or moving strokes | Pressure per outgoing segment; renderer uses document coordinates | Whole-stroke eraser incorrectly borrowed endpoint pressure; correct to renderer | P1 now |
| Undo/redo | Dot Undo observed; Redo offered but not executed | Document actions, EraseUndoAction, DeleteUndoAction | Existing gesture/history tests; mixed classroom flow validation outstanding | P1 |
| Circular eraser | K-ERASE-001 target labels only; no geometry tested | Circular cursor/disc, partial and whole-stroke, swept events | Preserve; pressure-width whole-stroke correction; no Kami algorithm assumption | P1 now |
| Eraser minimum | No size control seen, size unknown | Slider/keyboard controls; radius clamp 0.5–30 in handler tool settings | Existing small sizes; validate physical stylus precision | P1 validation |
| Partial erasing | Unknown | Centreline clipping plus erasure model; known excess thick-ink deletion | Separate compatible subtraction design needed; do not call pixel-accurate | P1 |
| Object erasing | “Erase all annotations” offered; exact targets unknown | Explicit Object mode deletes non-strokes incl. text/images/Answer Boxes | Existing undo/order/hidden-layer/border tests; preserve mode clarity | P1 |
| Overlap/hit testing | Unknown | Circular broad/narrow phases, overlap trees, grouped highlighter rendering | Existing sweeps/overlap tests; dashed/fill hit approximations remain | P1/P2 |
| Text/fonts | K-TEXT-001 size 14/colors, no box edited | GTK/Pango editor, font/size/color and Answer Box text | Verify IME, Unicode, alignment, transforms and PDF fidelity before additions | P1 |
| Lists/spacing | Unknown | Bullet/number continuation tests; text-format controls | Continuation already exists; spacing/spell-check expansion requires separate design | P2 |
| Selection | Select exists, all transforms untested | Existing selection handlers, rectangular/lasso/objects, moves/resizes | Validate actual cursor and multi-object history; no evidence to replace engine | P1 |
| Shapes | K-SHAPE-001 rectangle/ellipse/triangle/line/polygon offered | Existing lines/arrows/rectangle/ellipse/triangle/polygon and recognition | Restore disabled square recognizer cases; retain modifiers/fill/export | P1 correctness/P2 expansion |
| Smart Highlighter | K-MARKUP-001 text/freehand/box/strike/underline offered | Freehand, Word/Line snapping and PDF text tools | Existing features; test text/scanned/rotated PDF fallback | P1 validation |
| Answer Box | No demonstrated equivalent | Styled Text model, delete editing box, explicit guidance | Preserve bounds/history; mixed save/export acceptance | P1 |
| Answer Layers | No demonstrated equivalent | Layer audience metadata, hidden-on-load, visible write target | Privacy is UTN requirement; prevent hidden edits/student leaks | P1 |
| Student View | Presentation menu offered, no projector test | Separate audience-filtered view, freeze/blank/fullscreen | Real second monitor and DPI test remains | P1 |
| Prepare/Reveal | Screen masking offered; untested | Existing prepare/reveal, spotlight and curtain | Validate state after page changes/undo/reopen | P1 validation |
| Marking Mode | Annotation Bank exists; use unknown | Existing marking toolbar profile | Shared tool order implemented; validate fast marking workflow | P2 |
| Teacher Stamps | Annotation Bank label only, no equivalent proved | Built-in/custom teacher stamps | Check persistence/transforms/export of custom assets | P2 |
| Lesson Navigator | Page controls visible; deeper navigation unknown | Page labels/jumps and document navigation | Reorder/delete/current-page state and keyboard validation | P2 |
| PDF import | Blank PDF viewed; no import pipeline tested | Poppler-backed existing import | Synthetic mixed-content import and attachment fidelity checks | P1 validation |
| PDF export | K-PDF-001 blank export UI completion; file not checked | Cairo export, page options, classroom audience filtering | File integrity/content, answers visibility and failure handling are acceptance targets | P1 |
| Page edits/split/merge | Split/Merge and Add more pages offered, untested | Existing page operations; document-level split/merge workflow requires fresh inspection | Do not add duplicate UI; verify page insert/delete/reorder before split/merge proposal | P2 |
| Images/signature | Signature/Add media offered, untested | Existing image insertion and signature tool | Transparency/resize/undo/save/export check remains | P2 |
| Saving/reopen | Blank reload only | XOPP Save/Load, audience and eraser roundtrip tests | Nonempty classroom GUI flow and disk-failure recovery not fully verified | P1 |
| Read aloud/OCR | Read aloud Premium marker; text recognition menu | Windows voice tool, local external Tesseract OCR preview | Voice/device/installation validation; no selected-PDF synchronized reading | P2 |
| Comments/media | Comment modes inventoried, none created | Existing audio/document facilities; no claimed thread parity | Teacher need must precede new services or collaboration | P3 |
| AI/equations | Understand/Question/Equation named, functionality unknown | Existing local TeX equation pipeline, no new AI integration | Avoid speculative network features; dependencies and privacy design first | P3 |

The [roadmap](implementation-roadmap.md) specifies affected sources, risks and acceptance tests. Research lacks pen latency, eraser behavior, functional text/shapes, nonempty export, offline behavior and memory evidence; none can serve as a comparative benchmark.

Implementation addendum: the whole-stroke outgoing-pressure correction marked P1 now is implemented; 282 enabled unit tests and 29 GTK tests pass. The matrix otherwise records the audited base and deferred proposals. See [completion-report.md](completion-report.md).
