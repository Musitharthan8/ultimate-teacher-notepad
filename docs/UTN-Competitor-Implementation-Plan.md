# UTN implementation recommendations

Baseline: draft PR #19 after `d27d9a11c`. See [evidence and limits](REA-Competitor-Findings.md). Effort bands below are planning estimates, not delivery commitments: S = roughly 1–3 engineering days, M = 1–2 weeks, L = 3–6 weeks, XL = multi-stage work beyond six weeks. Device testing and compatibility repair can extend them.

## Ranked feature comparison

Scores: usefulness, reliability benefit and likely performance benefit are 1–5, higher is better. Effort/risk break ties; estimates are analyst judgments. No measured competitor performance is implied.

| Rank | Reference / behaviour | UTN evidence and gap | Independent work | Benefit U/R/P | Effort / risk |
| ---: | --- | --- | --- | --- | --- |
| 1 | Drawboard polygon-based partial erase | UTN trims widened centreline intervals; rendered probe proves excess visible loss | Prototype Boolean subtraction of visible stroke outlines, compare with per-stroke clip storage, settle compatible persistence before switching defaults | 5/5/3 | L–XL / high |
| 2 | Penly stylus experience; Drawboard modelled ink | UTN already has pressure, stabilizers and touch input; no comparable measured traces | Replay fixed input traces, preserve taps/endpoints/corners, evaluate optional modelling adapter separately | 5/4/4 | M–L / medium |
| 3 | Xodo consistent annotation manipulation | Multi-element EditSelection and Object Eraser exist; persistent grouping/overlap UX needs audit | Common object menu, property-capability map, deterministic overlap cycling, shared undo transactions | 5/5/2 | M / medium |
| 4 | Drawboard favourites and contextual controls | PR #19 already provides contextual panels and colour presets | Persist named tool presets and verify temporary eraser return semantics; retain accessible mode labels | 5/4/2 | M / low–medium |
| 5 | PDF24 processing + Sejda page organisation | UTN page controls and range export exist; no unified cross-source composition workflow established | Page-ID-based local organiser; reuse Poppler/qpdf; transactional reorder/delete/import; preview before save | 5/5/3 | L / high |
| 6 | Foxit printed OCR + reading geometry | Current OCR helper returns plain text from one image; Poppler already exposes text and rectangles for text PDFs | Background OCR with word boxes, confidence, reading order, source transform and selectable languages | 5/5/2 | L–XL / high |
| 7 | Bluebeam/Lumin local marking and anchors | Feedback boxes exist; persistent comment threads and source anchors are not established | Local comment model and sidebar; stable page/annotation IDs; original/marked views; explicit student export policy | 5/5/2 | L / high |
| 8 | Glean/Genio playback anchors | Notebook audio and Windows TTS exist; pasted-text reading is separate from PDF selection | Selection-to-speech service, cancel/pause/resume and range mapping; playback anchors separate from TTS tokens | 4/4/2 | M–L / medium |
| 9 | Heptabase source-backed cards | Feedback reuse is not a source-linked teaching-card system | Local cards with immutable source reference plus editable explanation, missing-source handling and reuse | 4/4/2 | L / medium–high |
| 10 | MagicSchool teacher-controlled structured outputs | Existing marking comments are not a rubric/total schema | Offline editable criteria, descriptors, score bounds and deterministic totals; no AI dependency | 4/4/1 | M / medium |
| 11 | Visual Paradigm attached connectors | UTN has stroke shapes/snapping; no connector relation model established | Start with labelled arrows; add explicit endpoint anchors only after element IDs/undo model are settled | 3/3/1 | L / high |
| 12 | StoryJumper page media | Static images and notebook audio exist; GIF/page narration lifecycle is absent | Bounded animated media with play state, page visibility lifecycle and static export poster | 3/3/2 | L / high |

## Immediate PR #19 scope

Keep the current eraser modes, hidden-layer guards, undo ordering and signature/read-aloud work. Add the optional visual diagnostic and evidence documentation without changing the application renderer. The diagnostic is excluded from the default build and installation.

Continue focused regressions for object-menu capabilities, hidden-layer transitions, tool-panel lifetime and Smart Shape classification. Two disabled square-versus-circle tests remain a known defect; do not claim that changing the circle threshold solves it. A corner-aware recognizer needs varied sampling, rotation, scale and jitter fixtures before enabling those cases.

Do not merge unfinished precision-eraser work merely to reduce branch count. The owner's standing permission permits merges and guarded history rewrites when needed; it does not make a known defect pass its acceptance gate. A fast-forward push is sufficient for the current work. Recheck the exact remote head before any history operation; use an explicit expected-SHA lease if a rewrite becomes necessary.

## Precision eraser design decision

Required result: removed region equals the intersection of existing visible ink with the swept eraser region. A fast drag is a union of discs/capsules, not isolated point samples. Apply erasing to the touched stroke, never as a page-wide white paint layer that also covers unrelated objects or later ink.

| Option | Benefit | Main constraint |
| --- | --- | --- |
| Keep splitting centreline intervals | Ordinary XOPP strokes remain editable; minimal change | Cannot express an internal hole or grazing notch in thick ink; fails the requested semantics |
| Store per-stroke vector clip regions | Preserve source points, width/pressure and independent erase history; potentially compact | Must update render, bounds, hit testing, selection transforms, clipboard, clone, undo, save/load and PDF export. Unknown extensions ignored upstream could resurrect erased ink. Not safe as a silent format extension |
| Convert visible outline to polygon regions and subtract sweep | Represents holes/notches directly; captured Drawboard web path shows this class of mechanism is practical | Need a robust licensed Boolean library or existing equivalent, winding/hole handling, pressure outline generation and a compatible editable element/export representation |
| Rasterise each touched stroke | Straightforward alpha subtraction and image-compatible persistence | Loses stroke editability; resolution/memory costs; unsuitable as an automatic default |

Recommended experiment: produce visible outlines from UTN's existing rendering geometry, subtract capsules with an independently selected Boolean implementation, and compare against the raster oracle. In parallel, test whether the result can be represented faithfully using existing XOPP elements. Do not copy Drawboard's tuning constants or proprietary handlers. No whole-engine replacement is justified by current evidence.

Acceptance gates before release:

1. Outside-sweep visible ink remains unchanged within a documented antialias tolerance; inside-sweep ink disappears. Include thick/pressure strokes, taps, dashes, self-intersections, filled shapes and highlighters.
2. Fast and slow traces with the same swept path agree. Test overlapping strokes and discontinuous input, with one undo transaction per gesture.
3. Exact undo restores source order/geometry/pressure; redo reproduces results. Closed paths and holes survive repeated edits.
4. XOPP reopen, copy/paste, transforms and PDF export preserve holes and do not resurrect erased ink. Test upstream Xournal++ compatibility explicitly; no silent format downgrade.
5. Benchmark p50/p95 event processing and repaint time on an identified Windows device using retained input traces. Record baseline and workload before setting a latency budget. No hardware latency claim is currently supported.

## Separate future work streams

These are branch plans, not implemented features. Create each implementation branch from the latest verified base when work begins; no empty future branches or unrelated systems were added to PR #19.

| Planned branch | First coherent slice / dependencies | Acceptance and regression tests |
| --- | --- | --- |
| `feature/utn-ink-precision-prototype` | Visible-outline subtraction versus clip persistence experiment; reuse diagnostic | Pixel oracle across scales, pressure/cap/join cases, immutable source data, edit round trips and export |
| `feature/utn-ink-replay-presets` | Recorded input harness around StrokeStabilizer/StrokeHandler; preset schema independent of UI | Tap/short-stroke retention, sharp-corner error, endpoint drift, discontinuous timestamps, preset restart and temporary tool restoration |
| `feature/utn-object-interactions` | Per-type capabilities over existing EditSelection; keep Object Eraser semantics | Mixed text/AnswerBox/image/ink selection; overlapping objects; duplicate/transform/delete/stack; undo/redo; restart preserves editability |
| `feature/utn-local-pdf-organiser` | Stable page IDs, virtualised thumbnails, command transaction layer, existing Poppler/qpdf | 30-page reorder/delete/add blank/import from second source; mixed sizes/rotation; undo; native annotations follow pages; original files unchanged; malformed/password errors; atomic output save |
| `feature/utn-ocr-geometry` | Cancellable local worker, optional Tesseract runtime/models, word boxes and transforms | Printed English defaults; selectable languages; record actual available models (do not invent British models); scale/rotation/crop mapping; reading order; confidence review; unchanged page image; no handwriting-accuracy claim |
| `feature/utn-local-marking-review` | Stable annotation/page IDs, local comments, sidebar and original/marked view | Anchors survive permitted transforms/reorder; deleted source handling; feedback reuse; privacy filters; no private-answer leakage in export |
| `feature/utn-teaching-cards` | Source digest/page ID/region/text quote plus editable notes and local library | Source relocation and revision mismatch; reusable copy without editing source; missing-source message; XOPP/library round trip |
| `feature/utn-reading-audio` | Shared selected-text spans for dictionary/TTS; playback states outside ink event path | Pause/resume/stop; Unicode and repeated words; close document mid-playback; token-to-region alignment under zoom/rotation; offline TTS; recording/ink concurrency |
| `feature/utn-rubric-templates` | Editable criteria/levels and bounded marks; native template store | Totals, fractional scores, missing marks, range validation, teacher overrides, deterministic export and undo |
| `feature/utn-labelled-connectors` | Labelled arrow first, then endpoint references if model supports them | Move/resize/rotate connected objects; undo/reopen; deletion fallback; no hidden dependency cycles |
| `feature/utn-lesson-media` | Bounded decoding; explicit page audio; GIF poster/play state | Moving/resizing during playback, closed-page cleanup, Student View freeze/blank, memory bounds and static PDF fallback |

## Investigation and implementation status

Delivered in this pass: captured Drawboard UI transitions and static mechanism evidence; Android capability assessment; limited Xodo/Sejda entry captures; official-source scouting for the wider roadmap; UTN eraser geometry audit and reproducible pixel diagnostic.

Not delivered: a precision-eraser fix, physical stylus benchmarks, complete reverse engineering of all thirteen apps, native competitor runtime tests, or any of the larger future systems. The existing Kami work and all PR #19 changes are preserved.
