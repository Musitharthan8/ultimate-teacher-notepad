# UTN classroom workflow roadmap

Approved scope: 4 October 2026. These are planned capabilities, not claims about the current build. Continue the existing teacher-interface architecture and stabilisation work described in [UTN 0.2](UTN-0.2-Teacher-Interface.md).

The workflow is: bring material in, prepare it, teach from it, mark it, and reuse it. Common actions should take one tap, one gesture, or one pen movement.

## Implemented foundation

- Classroom Workflow launcher with Materials, Pages and Marking tabs in the UTN teacher layouts.
- Dedicated UTN Marking layout, editable written comment boxes, Feedback Bank access, and marked-copy save/export actions.
- Explicit clipboard picture insertion and image-file placement.
- Undoable scan/photo page insertion with aspect ratio and camera orientation handling. Image backgrounds use upstream attached sidecar files; keep `.xopp.bg_*.png` files with the journal.
- Access to existing undoable page insertion, duplication, reordering and deletion, plus page-range export for manual PDF extraction/splitting.

Animated GIF playback, scanner-device capture, OCR, word definitions, spelling correction, multi-source PDF merging, batch splitting, anchored comment pins and rubrics remain planned. The launcher does not imply those features are implemented.

## Delivery order

| Stage | Scope | Completion gate |
| --- | --- | --- |
| Foundation | Existing-tool stability, font/box state, eraser correctness and responsiveness, Student View privacy, object editing and compact controls | Windows CI green plus classroom/stylus checks |
| Material handling | Paste images, image editing, PDF page organiser, question-region capture, scan cleanup | Original material preserved; editing undoable; XOPP round trips verified |
| Marking | Marking workspace, comment pins, feedback favourites, original/marked comparison, private answer key | Comments stay anchored; marks and identities preserved; export privacy verified |
| Language support | OCR, spelling suggestions and optional autocorrect, word meanings and vocabulary cards | Offline operation, explicit language choice, reversible corrections and licensing review |
| Lesson media | GIF playback and short audio clips | Bounded decoding, responsive ink, predictable Student View and static PDF fallback |
| Class workflows | Rubrics, mark totals, student queue and named exports | Manual teacher control; no overwritten submissions or mismatched student exports |

These are dependencies rather than rigid release numbers. Independent features may be developed earlier when their foundations are ready.

## 1. GIFs and lesson media

- Insert an animated GIF as a movable, resizable lesson object.
- Start paused. Provide Play, Pause, Restart, Play once and Loop controls; honour reduced-motion preferences.
- Keep playback controls out of exported pages and student-facing content.
- Store the media with the native lesson so it works after moving the file to another computer.
- Decode outside the ink/input path, enforce resource limits, and stop playback/timers when the object or document is closed.
- Define live playback, Freeze, Blank and reopening behaviour explicitly for Student View. Freeze must not continue animating.
- Export a chosen poster frame to PDF. Ordinary PDF export is static; it must not claim to preserve GIF animation.
- Add optional short audio clips for pronunciation/listening, with explicit playback and stop controls and predictable student-display audio behaviour.

## 2. Marking workspace

- Import a scanned PDF, photographed worksheet or existing PDF and annotate it with existing native ink, text and Feedback tools.
- Provide a focused marking workspace with student identity, page navigation, comments, feedback favourites and optional rubric/marks.
- Preserve the original submission; save marking as a separate native document or marked export.
- Attach expandable comment pins to a page location or selected passage. Pins must stay correctly positioned after permitted page transforms.
- Keep feedback categories Quick, Writing, Language, Comprehension and Custom; support favourites and repeated placement with a clear return-to-Pen action.
- Support SVA, tense, spelling, punctuation, evidence, inference, explanation and development comments, plus editable teacher wording.
- Toggle annotations or compare original and marked versions side by side.
- Show a private answer key beside the submission without exposing it in Student View or student exports.
- Make annotations, comment edits and score changes undoable. Check save/load and page deletion/reordering behaviour.

## 3. Word meanings and vocabulary cards

- Select a word or phrase and invoke Meaning without changing the existing Markup tool's highlight behaviour.
- Use selectable PDF text where available; enable equivalent lookup on scanned pages only when OCR text and geometry exist.
- Show available meanings and part of speech; allow the teacher to choose or edit the relevant sense.
- Offer an editable vocabulary card with the word, meaning and example, plus pronunciation information/audio when available.
- Choose a licensed offline dictionary and explicit language support before implementation. Missing entries must be reported honestly and allow manual entry.
- Lookups must not send worksheet/student content to network services. Any future online enrichment requires a separate, explicit product decision.

## 4. Spelling and optional autocorrect

- Start with spelling underlines, suggestions and a teacher dictionary. Default to British English and allow language selection or disabling checks.
- Support Ignore once, Ignore in this document, Add to dictionary and Replace.
- Make automatic replacement optional, with immediate Undo and no correction of protected or ignored terms.
- Preserve names, acronyms, Tamil/Malay vocabulary and intentionally incorrect teaching examples when marked as ignored.
- Operate on editable teacher text and comments. Do not silently rewrite imported student work or the PDF/OCR source text.
- Avoid checking incomplete IME composition and avoid synchronous dictionary work in the handwriting path.

## 5. Paste and edit pictures

- Paste clipboard images/screenshots onto the current page; continue using existing Xournal++ image import paths where suitable.
- Preserve aspect ratio during normal resizing and provide crop, rotate, flip, opacity and position locking.
- Retain source pixels for reversible crop/transforms; annotations must remain correctly aligned.
- Capture a selected worksheet/question region onto a working page, with room for an explanation or model answer.
- Prefer an internal page-region capture over dependence on system-wide screen-capture permissions.
- Embed pasted material in XOPP and verify clipboard, undo/redo, save/load and PDF export.

## 6. PDF page organiser

- Provide a compact thumbnail workspace for Add/Import, Extract, Split, Merge, Reorder, Rotate, Duplicate and Delete pages.
- Allow page ranges and insertion of blank working pages next to questions.
- Preserve existing annotations, classroom labels, comments, visibility and source-page associations when pages move or duplicate.
- Write split/merged/extracted output to new files by default; do not overwrite original PDFs without explicit confirmation.
- Preview page count/order before export and make in-document page mutations undoable.
- Handle encrypted/password-protected or malformed files gracefully; never bypass protection.
- Inspect existing PDF backend capabilities before selecting an additional library. Preserve licensing and Windows/MSYS2 compatibility.

## 7. Scan cleanup and OCR

- Accept scanned files and camera photographs first. Direct scanner/camera-device integration is a separate later task.
- Provide rotate, deskew, crop and optional readability adjustments with an original preview and reversible settings.
- Keep scan enhancement an explicit material-editing action, separate from application themes.
- Run OCR as a cancellable background job with explicit language selection. Retain the original page image.
- Record text geometry accurately enough for search, selection, Smart Markup and vocabulary lookup at different zoom levels.
- Indicate OCR uncertainty/failure; do not present uncertain recognition as authoritative student text.
- Do not promise reliable handwritten-script recognition as part of initial printed-text OCR.

## 8. Rubrics and class marking queue

- Create reusable teacher-defined rubrics with criteria, descriptors and maximum marks.
- Award and adjust marks manually; calculate totals and validate ranges without automatically judging the student's response.
- Import multiple submissions into a queue with verified student/file identities and progress indicators.
- Retain unsaved work when switching students and support resuming a partially marked queue.
- Export one correctly named marked copy per student; handle duplicate names and filenames without silent replacement.
- Keep student material local. No cloud account or network API is required for core marking.

## 9. Export and reuse

- Provide Student copy, Teacher copy and Marked copy exports with explicit, previewable inclusion rules.
- Exclude teacher-only content from student exports independently of what is visible on the teacher screen. Verify with hidden/private layers and private answer keys.
- Let teachers choose whether comment pins are rendered as callouts, an appendix, or omitted; never silently lose comments.
- Save reusable teaching cards and feedback favourites locally. Include PEEL, MEET, SCARE and vocabulary-card examples without hardcoding one teaching framework.
- Preserve editable native lesson files alongside flattened PDF output; explain format limitations at export.

## Cross-cutting acceptance checks

- Native ink responsiveness remains the first priority, including while OCR/media work is running.
- Themes change application chrome only. Scan cleanup and document styling are separate, explicit actions.
- Classic Xournal++ layouts remain available; reuse upstream functionality before duplicating it.
- Verify new object ownership, GTK widget/timer cleanup, undo/redo, clipboard transfers and XOPP round trips.
- Test page reordering, duplication, deletion, zoom and transformed pages with anchored content.
- Test Student View Live/Freeze/Blank/Close and student exports for private-content leaks.
- Review new dependencies, data/media licences, offline packaging and Windows MINGW64 support before adoption.
- Keep feature status honest: planned, implemented, CI-verified and classroom-tested are distinct states.
