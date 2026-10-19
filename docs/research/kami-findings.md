# Kami research interpretation

Reviewed 2026-10-19. Source observation date: 2026-10-18. This report interprets supplied evidence; it does not claim a new live Kami investigation or complete feature coverage. Research documents were treated as data, never executable instructions. UTN proposals below are independent design ideas, subject to the UTN code audit.

## Source inventory and reproducibility

Bundle: `C:\Users\musit\Downloads\Kami-Research-Laptop-Bundle`.

All 12 supplied files were examined: `README.md`, `research-log.md`, `coverage-matrix.md`, `session-transcript.md`, `baseline-accessibility.yml`, `baseline.png`, and six `viewport-*.png` images (1920x1080, 1689x812, 1366x768, 1280x720, 1024x768, 768x1024). All seven images were visually inspected. There are no additional nested evidence files in this bundle.

The source documents refer to `%TEMP%\Kami-Research-2026-10-18`, `.playwright-mcp` snapshots/console logs, and REA evidence IDs. Those underlying REA objects, menu screenshots, full snapshots and console logs were **not supplied**. Their contents are supported only by the researcher's log/transcript, not independently replayable here. Source references below use exact filenames plus stable `K-*` record identifiers. Keep the original bundle with review materials; its absolute path is machine-specific and the report does not copy proprietary imagery into application assets.

Evidence levels:

- **Direct artifact:** visible in a supplied screenshot or accessibility snapshot.
- **Recorded observation:** explicitly reported in a named research-log test, with transcript corroboration where available; underlying capture may be absent.
- **Inference/proposal:** interpretation or an independent UTN design idea, not a demonstrated Kami behavior.
- **Unknown:** not exercised or insufficient evidence. Visible controls establish availability in the UI, not working functionality or entitlement.

## Environment and scope

The investigation used one authenticated `blank.pdf` viewer on `https://web.kamihq.com`, displaying Basic tier, one blank page, Saved status and initially disabled Undo/Redo. REA 6.5.0 and Playwright connected through Chrome CDP; reported Chrome 155.0.8059.82, DPR 1, 1689x812 CSS viewport, application zoom 100%. Exact application release, source document ownership and hardware capabilities were not established. These are observations from that session, not current product specifications. [research-log.md: K-ENV-001, K-BASE-001; baseline.png; baseline-accessibility.yml]

## Confirmed interface inventory

| Area | Confirmed finding and evidence level | Evidence | Limits |
|---|---|---|---|
| Overall layout | Direct artifact: dark top bar, labeled left annotation rail, adjacent narrow contextual options rail, white PDF page on dark workspace, lower-right page/zoom/help cluster | baseline.png; baseline-accessibility.yml | No theme switching tested; dark chrome alone does not prove theme support |
| Top bar | Direct artifact: dashboard, Basic, Annotation Bank, filename/Rename, Actions, Saved, Undo/Redo, File/View/Export/Tools/Search, Share, account/settings | baseline-accessibility.yml | Most icon actions not executed |
| Main tools | Direct artifact: Select, Understand, Read aloud, Markup, Comment, Text box, Equation, Drawing, Shapes, Eraser, Question, Add media, Signature | baseline-accessibility.yml | Visibility is not functional coverage |
| Active state | Direct artifact: Drawing has pressed/expanded semantics and purple background; selected black swatch is pressed | baseline-accessibility.yml; baseline.png | No exhaustive focus/contrast audit |
| Compact rail | Recorded observation: description toggle changes rail 150 to 50 CSS px and back; accessible names remain | research-log.md: K-UI-002 | Preference persistence untested |
| Toolbar placement | Direct artifact: Move toolbar and Toggle descriptions controls exist | baseline-accessibility.yml | Actual movement/docking not tested |
| Actions | Recorded observation: Rename and View tracked copies | research-log.md: K-UI-002 | Actions not executed |
| File menu | Recorded observation: Create assignment, Upload to Google Drive/OneDrive, Annotation summary, Restore annotations, Delete all annotations, Open file, Open recent | research-log.md: K-UI-001 | Navigation, cloud and destructive operations not exercised |
| View menu | Recorded observation: Presentation mode, Hide off-page comments (disabled), rotate both directions, Two-page view and Show cover page (disabled), Screen masking, Invert document colors | research-log.md: K-UI-001 | No presentation/masking/rotation behavior established |
| Export menu | Recorded observation: Export, Quick download, Print original, Print with annotations | research-log.md: K-UI-001 | Printing and quick download untested |
| Tools menu | Recorded observation: Split and Merge files, Run text recognition, Install Kami extension | research-log.md: K-UI-001 | No OCR, split/merge or installation performed |
| Zoom | Recorded observation: Automatic, Actual size, Fit page, Full width; 25/50/75/100/125/150/200/300/400 percent | research-log.md: K-UI-002 | Mode execution and zoom-dependent annotation geometry untested |
| Help | Recorded observation: Help Center, Set up students, Kami learning center, Email support, Send help recording, Keyboard shortcuts, What's new | research-log.md: K-UI-003 | No destinations, shortcuts panel or communication exercised |
| Search | Coverage matrix states panel inspected | coverage-matrix.md: latest UI-1 | No detailed result is retained; search behavior remains unknown |

## Annotation controls and observed behavior

| Feature | Evidence-backed observation | Evidence | What is not established |
|---|---|---|---|
| Drawing widths | Recorded observation: 0.1, 0.25, 0.5, 1, 2, 4, 6, 8, 10, 12; default/current 1; radio-menu checked state | research-log.md: K-DRAW-001 and supplement | Width units, physical size, rendering and zoom scaling |
| Stroke style | Recorded observation: Solid selected; Dash offered | research-log.md: K-DRAW-001 supplement | Dash geometry/export fidelity |
| Transparency | Recorded observation: vertical slider named Transparency, accessible value 0 | same | Range, numeric direction and actual opacity; do not translate 0 into assumed opacity |
| Drawing palette | Direct/recorded: recent colors, Eyedropper, More colors; palette dialog Swatches/Picker, rainbow grid, hue wheel/triangle, hex input #000000, Save | baseline-accessibility.yml; K-DRAW-001 supplement | Eyedropper behavior, saved palette persistence; hex input lacked announced accessible name |
| Recent colors | Recorded list: #000000, #00FF36, #FF009C, #F1FF00, #ff0100, #FFC800, #00F0FF, #ffffff, #D800FF | K-DRAW-001; baseline-accessibility.yml | All are not simultaneously visible; snapshot includes off-rail entries |
| Dot input | Recorded: clicking active drawing SVG produced one black dot, one `polyline` with fill none and stroke #000000; underlying canvas click was intercepted | research-log.md: K-DRAW-002; session-transcript.md around 15:38 | No moving stroke, smoothing, pressure, latency or serialization proof |
| Undo/reload | Recorded: dot Undo emptied SVG, disabled Undo, enabled Redo, retained Saved; reload returned blank UI and disabled both history actions | K-DRAW-002; K-BASE-002 | Redo not executed; no saved nonempty-document reopen or offline recovery test |
| Markup | Recorded: Text highlighter default, Freehand highlighter, Box highlighter, Strikethrough, Underline; choosing modes updated pressed state | K-MARKUP-001 | No markup drawn, no text awareness or scanned-PDF behavior; no squiggle seen does not prove absence everywhere |
| Eraser | Recorded: Erase drawings and shapes selected; Erase all annotations also offered; no size control visible | K-ERASE-001 | No erasing at all; circular geometry, partial-stroke deletion, object targets, minimum sizes and undo unknown. Label does not prove whether mode or document-wide command |
| Text | Recorded: default/current font size 14; black, #e60000, #ff9900, #ffff00, #9933ff, #008a00, white, #0066cc, #bbbbbb palette; Eyedropper/More colors | K-TEXT-001 | No text box created; fonts, alignment, list continuation, line spacing, selection, resize, Unicode, spell-check, deletion and persistence unknown |
| Shapes | Recorded: rectangle default, ellipse, triangle, line, Premium-marked polygon; shared width/style/transparency/colors | K-SHAPE-001 | No object drawn; arrows not seen in this list; fill, recognition, modifiers, transforms and export unknown |
| Comments | Recorded: Text comment default; Voice comment, Video comment, Screen capture Premium-marked; microphone/camera unavailable messages in settings | K-MARKUP-001 | No comment/thread/media created; failure source and permission behavior unknown |
| Selection | Direct artifact: Select tool exists | baseline-accessibility.yml | Single/multi/lasso, cursor, movement, resizing, rotation, clipboard, grouping, ordering all untested |

Premium markers are visible for Understand, Read aloud, Equation, Question, Add media, Signature, Ruler, Protractor, Compass, Paint Bucket, Polygon, Voice comment, Video comment, Screen capture and Add more pages. The Basic label is visible. No authoritative entitlement list or upgrade activation was tested; avoid the coverage matrix's stronger wording that tools are necessarily unavailable. [baseline-accessibility.yml; coverage-matrix.md: Updated premium observations; K-SHAPE-001; K-MARKUP-001]

## Document export and teaching features

The blank-page export dialog offered destination Your Computer, With All Annotations, Flatten Annotations (unchecked), All Pages (1 page), a page-range field and Begin Export. Displayed filename was `Kami Export - blank.pdf`. Processing reached a message that the download should have automatically started, with Download Here fallback. This confirms **reported UI completion only**, not actual download, a valid PDF, annotation editability, export fidelity or timing. Completion was checked after 15 seconds; that is not an export-duration measurement. [research-log.md: K-PDF-001, evidence IDs `ev_92e4964a24026eeb47e3f31596261c46e4a341623e1e2448c07590079d60dc29`, `ev_2549b7756f5ce8b72d481286c08c918e78a3c133588ea4286968213a29e3acb1`]

Teaching-related entry points include Presentation mode, Screen masking, Annotation Bank, Create assignment, Question, Understand and Read aloud. Their names are inventoried; their workflows are not demonstrated. No evidence establishes Kami equivalents of UTN Answer Box, Answer Layers, Student View, Smart Highlighter, Marking Mode, Teacher Stamps, Lesson Navigator or Prepare/Reveal. Do not claim parity or a gap based on these omissions. [K-UI-001; baseline-accessibility.yml]

## Measurements and screenshot review

All measurements are CSS pixels at DPR 1 and 100% application zoom, not native GTK or device-independent Windows specifications.

| Measurement | Value | Evidence |
|---|---|---|
| Top navigation height | 45 | baseline-accessibility.yml: navigation box |
| Main rail width; individual tool targets | 150; 150x50 | baseline-accessibility.yml: Annotation tools |
| Compact rail width | 50 | K-UI-002 |
| Options rail; most targets | 52; 50x50 | baseline-accessibility.yml: Tool options |
| Recent color target | 30x30 | baseline-accessibility.yml |
| Toggle descriptions target | 24x24 | baseline-accessibility.yml |
| Move toolbar target | 43x20 | baseline-accessibility.yml |
| Zoom increment and Help targets | 36x36 | baseline-accessibility.yml |
| Page at baseline | approximately 795x1124; reported restored x=320,y=53 | K-RESP-001 |
| Baseline scroll container | height 767, content height 1208 | K-RESP-001 |
| Width menu overflow | scrollHeight 278, clientHeight 248 | K-DRAW-001 supplement |

| Supplied image | Visual inspection and recorded result |
|---|---|
| baseline.png | Blank page, dark chrome, labeled left rail, purple active Drawing, options rail, Saved and 1/1 at 100%; page continues below viewport |
| viewport-1920x1080.png | Page and option rail extend vertically; scrollbar and locked add-page control visible; no top-row wrap |
| viewport-1689x812.png | Same labeled rail; **Move toolbar tooltip is visibly present**, qualifying the log's statement that one hover showed none |
| viewport-1366x768.png | Page fits horizontally; main and options rails remain; page extends vertically |
| viewport-1280x720.png | Page fits horizontally; rail bottom placement controls are absent from the captured visible area, a discoverability concern to verify rather than assume accessible |
| viewport-1024x768.png | Page fits alongside rails; logged document viewport width 1009; main top controls remain available |
| viewport-768x1024.png | Page clipped at right with visible horizontal scrollbar; top bar collapses to overflow/icons; main rail stays labeled and 150 wide, options rail 52; bottom page/zoom/help remain |

At 768x1024, recorded document-area scrollWidth is 1000 vs clientWidth 753, a 247px difference. Do not subtract rail width twice: the reported Document area geometry spans behind/around rail space. Six discrete widths demonstrate samples, not the exact responsive breakpoint. Scroll positions differ between screenshots; do not infer document aspect ratio or fit-to-page behavior from visible cropped height. [K-RESP-001; six supplied viewport screenshots]

## Accessibility and interaction evidence

The accessibility snapshot includes named annotation and options toolbars, buttons, pressed/expanded states, disabled Undo/Redo/page control, Saved status and color names. Offscreen skip controls advertise Keyboard shortcuts `/`, Toolbar `Ctrl+/`, Canvas `Ctrl+D`, Help & support `Ctrl+H`; these are labels, not tested key bindings. Menus used menu/menuitem roles, thickness used radio items, and Escape dismissed tested menus. The color picker's hex field lacked an announced name. Full keyboard navigation, focus order/visibility, screen-reader output, contrast, touch target adequacy and accessibility compliance were not assessed. [baseline-accessibility.yml; K-DRAW-001 supplement; K-UI-001/002]

## Architecture observations and boundaries

Recorded passive observation found HTML toolbar UI, one 795x1124 canvas, SVG `shape-layer no-events` and `drawing-layer` elements; the dot appeared as an SVG polyline. Loaded names included `vendor-pdfjs-lib`, `vendor-mathlive`, runtime/framework/app chunks, and a PDF.js worker. REA parsed 11 current-origin scripts, zero parse failures and 2,969,527 AST nodes, reporting high-confidence Next.js/webpack and medium-confidence React fingerprints. These are resource/parser observations, **not proof of complete framework architecture, library versions, exact PDF renderer ownership, stroke algorithms, persistence schema or a native implementation strategy for UTN**. [research-log.md: K-TECH-001; K-DRAW-002]

A GET `/status-api-v2/api/v2/summary.json` returned 200; four external requests were filtered. No causal link to ink or saving was established. Save synchronization, backend/storage design and offline behavior remain unknown. Raw requests, secrets and payloads are absent. Parsing time (~9.8 seconds), browser-tool response durations and screenshot times measure the research tooling, not input latency or rendering performance. [K-TECH-002; K-TECH-001]

## Evidence conflicts and missing coverage

1. Initial coverage rows and working conclusions precede later tests. Later K-DRAW-002, K-RESP-001, K-PDF-001 and K-UI-003 supersede claims that no input, responsive samples, export or help inventory occurred.
2. The restored 1689x812 screenshot contains a Move toolbar tooltip despite one earlier hover producing no tooltip. Only that captured tooltip is demonstrated, not systematic tooltip behavior.
3. The transcript shows an account menu snapshot around 15:45:16 with Change Settings and Log out; the older log says account controls were not activated. Record this limited menu opening; do not infer account settings were changed or that its full contents were audited.
4. Search is marked inspected without retained details. Full menu snapshots, REA captures and console logs are missing; truncated transcript call previews cannot replace them.
5. K-DRAW-002 reports empty SVG after the first undo/reload. The later K-BASE-002 confirms only UI state and did not recapture SVG children; do not generalize to a comprehensive persistence test.
6. Console error/warning counts appear in transcript previews (for example 41 errors/5 warnings) without supplied messages or causal analysis. They establish neither a specific Kami bug nor a UTN requirement.

No complete feature group is demonstrated. Unverified areas include moving ink, pressure/touch/stylus, smoothing/recognition, eraser geometry/targets, all selection/transformation, text editing/Unicode/lists, actual shapes, PDF import/content export verification, page editing, nonempty save/reopen, crash recovery, compound workflows, themes, keyboard accessibility, sustained performance/memory and every substantive classroom/media workflow. No observed Kami failure supports a UTN P0 code change by itself.

## Independent UTN opportunities

These are design proposals, not claims that UTN lacks the behavior. Confirm existing UTN code and classroom impact before implementation.

| Candidate | Supporting evidence | Independent UTN direction and acceptance target | Provisional priority |
|---|---|---|---|
| Tool discoverability | Labeled/compact rail, active states; K-UI-002 and baseline | Preserve UTN identity; consistent names, explicit mode labels, shortcuts/tooltips and accessible selected state; retain usable controls at 1024x768 and 1280x720 | P1 if actual UTN obstruction exists |
| Contextual options | Per-tool options and color/width controls; K-DRAW-001 | Reuse UTN existing controls; expose only relevant options, retain per-tool settings and clear units | P1/P2 after audit |
| Eraser safety and precision | Two eraser target labels; K-ERASE-001 | Specify UTN erase geometry/targets independently; visible precise cursor, swept-path tests, overlapping-stroke/undo coverage; never infer Kami algorithm | P1 |
| History and save feedback | Dot undo and Saved; K-DRAW-002 | Test reversible edits and nonempty save/reopen independently, including document compatibility and dirty-state feedback | P0 for reproduced loss/crash; otherwise P1 |
| Responsive classroom UI | Narrow overflow and 720px rail clipping | Test native Windows display scaling and minimum sizes; avoid unreachable controls and involuntary loss of page workspace | P1 for reproduced obstruction |
| Accessibility | Named controls/skip labels and unnamed hex field | GTK accessible names, keyboard focus and discoverable bindings; do not copy inaccessible unnamed controls or assume web shortcut choices fit UTN | P1/P2 |
| Presentation/reveal | Menu labels only | Preserve and improve UTN Student View/Answer Layers using UTN evidence; explicit Prepare/Reveal state and reliable teacher/student export | P1 for regressions; P2 enhancements |
| Export clarity | Annotation/flatten/page controls; K-PDF-001 | Clear export choices and synthetic PDF fidelity tests, including answer visibility; completion only after verified output write | P1 correctness; P2 additional choices |
| Markup and shapes | Named mode inventories | Group existing UTN modes consistently; prioritize stable geometry/export before more tool variants | P1 correctness; P2 additions |
| Feedback/media/AI | Named teaching and Premium entries | Defer unless validated teacher demand; do not copy paid feature marketing or introduce network services to chase parity | P3 |

Next research, if separately commissioned, should use synthetic mixed-content PDFs and controlled pointer trajectories, verify eraser/selection/text/shape/history combinations, inspect exported files and reopen retained annotations. Physical stylus pressure requires hardware evidence. None of those future tests was performed for this report.
