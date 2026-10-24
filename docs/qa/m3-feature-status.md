# M3 integration status

Audit started 2026-10-23 from `f4e54c4d9` (M2 plus approved M3 requirements). This is an engineering inventory, **not milestone acceptance**. Existing test results cited by older documents require fresh execution on this branch.

| Feature | Status at audit | Evidence / remaining gate |
| --- | --- | --- |
| Pen, pressure, brush/width/colour/style controls | Implemented, additional verification required | ToolHandler, PenInputHandler, ToolUtnContextBar; hardware pressure, prolonged input and responsiveness outstanding |
| Highlighter and semantic modes | Implemented, additional verification required | ToolSmartHighlighter, HighlightModes; source preserves modes; native interaction outstanding |
| Partial ink / whole-stroke / deliberate object erasing | Implemented, additional verification required | EraseHandler and pixel-oracle tests; M2 register describes current-layer-only semantics. Hidden/private content must remain protected |
| Universal erase across visible editable layers | Partially implemented | Existing erase targets selected layer. No broad layer-policy change authorised by an unexplained field symptom alone; inspect/test visibility policy before changing |
| Smart Shape | Partially implemented | Existing imperfect synthetic corpus and handwriting guards; real stylus traces, ellipse recall and speed/zoom validation outstanding |
| Selection and object editing | Implemented, additional verification required | EditSelection; history, masked ink and mixed-object tests exist |
| Shapes & Lines | Implemented, additional verification required | ToolTeachingKit, ShapePicker; native selection/contextual popover integration outstanding |
| Active-tool tap opens anchored options | Missing across primary tools | ToolButton and ToolSmartHighlighter still expose split-arrow options; Text reselects its mode |
| Labels beneath rail icons | Implemented and automated tested in first M3 batch | All 13 primary captions; Show/Hide/Automatic; 1280→1024→1280; active-tool and inspector-state preservation. Actual GTK/native captures reviewed; scale/translation matrix pending |
| Top-level Classroom menu | Missing | Existing Classroom workflow is a toolbar launcher; standard menu XML lacks Classroom |
| Inspector pin/collapse/resize/persistence | Implemented, additional verification required | MainWindow workspace modes and existing GTK test. Settings should not change active tool |
| Spotlight / Curtain recovery | Implemented, additional verification required | M2 toggle, Escape, tool switch and visible exit banner tests; fresh native exercise outstanding |
| Ordinary text and Answer Box multiline/list editing | Partially implemented | TextEditor, TextEditorKeyBindings; literal list prefixes and known paragraph-model limitations. Enter/Shift+Enter, IME and reload need fresh reproduction |
| Handwriting to editable text | Missing | No selected-ink recognition engine/preview/undo integration established; engine viability/languages/licensing unassessed |
| Text formatting and spelling | Partially implemented | Existing formatting controls; verify optional GtkSourceView/spelling build dependencies and packaging |
| Answer Boxes / Feedback Bank | Implemented, additional verification required | ToolTextMode, ToolTeacherStamp; mixed-edit/persistence GTK coverage exists |
| Teacher stamps and drawn signatures | Implemented, additional verification required | ToolTeacherStamp, SignaturePad; native insertion, save/export and undo checks outstanding |
| Marking workspace | Implemented, additional verification required | Existing UTN Marking toolbar/layout; verify all tool states and accessibility |
| Hide / Reveal and private layers | Implemented, additional verification required | LayerAudience, PrepareReveal and Student View tests. Freeze retains previously shown frame; Blank overrides Freeze |
| Student View confidentiality | Implemented, additional verification required | Existing model/render/GTK checks; distinguish ordinary visible-layer export from student-safe export |
| Windows read-aloud | Partially implemented | Async Windows SpeechSynthesizer via PowerShell, rate/volume/pitch and voice controls; actual voice/cancellation tests outstanding |
| Voice typing/control | Additional audit required | Do not infer voice-input support from read-aloud controls |
| Dictionary | Partially implemented | Online dictionaryapi.dev lookup via async process; Internet required; no offline dictionary established |
| Scanned-document OCR | Blocked in self-contained package | Existing external Tesseract path; source explicitly says installer does not bundle engine/models. Packaging/licensing/languages and resulting editable text require validation |
| PDF page insertion/duplication/reorder/delete | Implemented, additional verification required | Existing page controls/scan import and undo paths |
| PDF extraction/splitting | Partially implemented | Existing page-range PDF export; dedicated integrated workflow outstanding |
| PDF merging | Additional audit required | Do not claim multi-file merge from import or page-range export |
| Images / scan-photo page insertion | Implemented, additional verification required | Clipboard/file image and scan page paths; sidecar portability must be checked |
| Animated GIF / lesson video | Missing or unverified | Existing roadmap explicitly defers animated media; no functioning playback proven |
| Lesson navigation / reusable materials | Partially implemented | ToolLessonNavigator and templates; end-to-end reuse workflow needs audit |
| Native-file saving / reopen and PDF export | Implemented, additional verification required | Existing round-trip, masked ink and audience tests; fresh build and export gate pending |
| Windows portable build | Implemented packaging, additional verification required | Existing CI and `scripts/test-utn-portable.ps1`; no fresh M3 package verified |
| Windows installer | Implemented packaging, additional verification required | Existing Windows setup workflow; clean install/upgrade/uninstall gate pending |
| Light / dark / high contrast | Implemented, additional verification required | Existing design tokens, theme CSS and historical native screenshots; fresh screenshot matrix pending |
| Repeated stylus strokes lag/freeze | **Release blocker: unresolved** | Preserved `diagnostics/utn-hang-2026-10-23` report records Windows Application Hang, no dump/root cause. No performance claim from pressure logs |
| Independent Claude review | Initial static audit completed; final candidate review pending | Separate detached worktree; independently invoked investigation/performance/privacy skills. See `m3-claude-initial-audit.md`; no claim of GUI/performance execution by Claude |

## Acceptance gate

No request for minor owner testing. No owner release candidate until functional, privacy, persistence, prolonged-input, native visual, packaging and independent review gates are complete. Only hardware-dependent stylus/projector and real teacher usability checks belong in final owner acceptance.

## Resumed batch update � 2026-10-24

Active Pen, Area, Eraser and Highlight controls now open their existing anchored options via primary click or keyboard; switching tools closes options. Highlighter preferences survive incidental resets. GTK tests cover matching release and action-state counts; native keyboard/focus/ATK and complete professional options content remain unverified or incomplete.

Student exports now default to audience-filtered content for PDF, progressive/range exports and images; print installs the same predicate. Explicit Teacher copy is available in custom export. Native editable lessons retain private data. Six real-file unit regressions and one dialog regression pass; see m3-export-privacy.md for exact scope and limits. Full suite: 327 units / 41 GTK. Initial matrix rows above are superseded by these results, not by an assertion that M3 is complete.

Top-level Classroom menu, Enter/Shift+Enter refinements, handwriting engine, remaining full options redesign, packaging and final independent review remain open. Hardware-stylus freeze remains release-blocking. Native GUI testing is paused by explicit user instruction.
