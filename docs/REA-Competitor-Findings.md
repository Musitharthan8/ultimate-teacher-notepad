# REA competitor findings

Investigation date: 15 October 2026. UTN baseline: PR #19, `d27d9a11c198bb22070df71b1d9203d32c07ee00`. Existing Kami findings were reused, not repeated.

## Evidence boundaries

This is a first investigation pass, not a claim that all requested applications or features have been exhaustively reverse engineered. Drawboard has substantive browser-runtime and captured-code evidence. Penly has capability checks and developer documentation only. Xodo and Sejda have limited page captures plus documentation. The remaining applications have preliminary official-documentation findings and explicit runtime gaps. No latency, palm-rejection, pressure-device, memory, or competitor erase-precision benchmarks were obtained.

REA 6.5.0 responded successfully. Browser scenarios, passive CDP inspection, and static web-bundle analysis worked. Windows native UI observation is unsupported by this REA provider. Android analysis is blocked by missing ADB, apktool, and the caller-supplied JADX MCP JAR. No authorised Penly APK was supplied. A bounded search of the two standard Program Files directory names found no matching competitor directories; that is not proof that the applications are uninstalled. Drawboard AppX enumeration was denied.

Target content was treated as evidence, not instructions. No competitor code or branding was incorporated into UTN. No account was created, paid feature unlocked, or private worksheet uploaded.

## 1. Drawboard PDF — primary investigation

### Reproducible runtime observations

Official entry point: [Drawboard download page](https://www.drawboard.com/download) → web application at `https://pdf.drawboard.com`.

REA launched temporary Chrome profiles and performed these declared actions: skip login, open the supplied readme sample, wait for rendering, open Free-form, select Eraser, and select the partial-mode radio. These changed only disposable sample sessions.

| Evidence | Observation |
| --- | --- |
| `ev_a4be62747c321d23bd0ab84e59bdcc7efa7d366a078d6061c07cc6cd3457ec93` | Guest use is explicitly offered alongside sign-in. |
| `ev_8bc77ced5801a8f6759c39adf72077e27b565e4b838fce30dff590aaa21d7979` | Sample viewer exposes tool categories, favourites, compact/expanded toolbar control, page/zoom controls, and pages/search/bookmarks/annotation-history/layers panels. |
| `ev_7f2cfebc755f1cbf3b12e45f41e1ce731b54b795b0a98efd5c3fa2e246e474b1` | Eraser opens adjacent options. DOM identifies a checked `fullEraser` and unchecked `partialEraser`. Both radio controls lacked accessible names in the captured tree. |
| `ev_f015a7cddfe6e977b9ce436ad7e6cb294b38b33fd43e49a1c39c25f8a5906779` | Clicking the partial radio changes its DOM `aria-checked` to true and the full radio to false. This proves the UI transition, not erasing precision. |

One earlier partial-radio attempt timed out because the selector incorrectly assumed a button element. Inspection showed a div with radio semantics; the corrected selector succeeded. A capture taken before the sample finished loading showed zero pages, so it was not used as document-state evidence.

### Captured implementation evidence

REA attached to a separately launched, empty-profile, loopback-only Chrome session. `analyze_web_bundle` parsed all 12 captured scripts with zero parse failures: `ev_27ef10bd9e4f98613f5419d451a32ca38af15ea952d0953ac3e0471cecc98c77`. This is complete examination of that capture, not complete application coverage.

A subsequent source-inclusive page inspection exceeded the MCP response budget. REA retained the complete record under `ev_5b740de5899918e1e834353cadb3d442553ade708743305b1dd73b49816744f2`; it was exported locally and inspected without repeating the producer. Raw proprietary source is kept in the local working evidence only, not checked into UTN.

Primary captured bundle: `https://pdf.drawboard.com/assets/appIndex-CFzGbICB.js`, SHA-256 `8dfaab48110e8110f756fef157fb690f065e6c70b459d3de5b066c43bdcd2c3e`.

- **Partial erasing:** the handler registered as `partialErase` obtains each annotation's polygon, calls a helper resolving to polygon difference, transforms the result, and replaces the annotation's geometry. Empty results remove the annotation. The candidate selection path filters partial erasing to ink, whereas whole erasing can expand grouped annotations. This establishes a static polygon-subtraction path in this captured web build. It does not establish exact geometric tolerance, export fidelity, runtime execution of that handler, or the Windows application's algorithm.
- **Coordinate provenance:** the relevant minified handler is at line 221, zero-based UTF-16 column 235904; the difference wrapper is at line 2, column 25825. These coordinates belong only to the digest above.
- **Handwriting:** the same bundle imports an Emscripten-style wrapper and a WebAssembly module for stroke modelling. Its setup constructs a `StrokeModeler`, and code paths call reset, update and predict. Configuration fields cover wobble smoothing, spring/mass behaviour, output sampling and Kalman prediction. Prediction code checks velocity and prediction error. Configuration begins at line 91, column 124107; update and predict references are at columns 126681 and 127381. These are observed source paths, not measurements of latency or proof of which path a physical stylus used.
- Wrapper identity: `ink-stroke-modeler-58-ddMhm.js`, SHA-256 `16d9aa2f1ce57d9c6aedaa56b3abfba4b5689ecccaeae95ad5bea3ba7bfac4a6`. The captured wrapper resolves and instantiates the named WASM resource. The WASM itself was not decompiled.
- Bundle detectors inferred React and build-runtime fingerprints. Mixed webpack/Vite detections should not be interpreted as a definitive architecture inventory.

The public [Google Ink Stroke Modeler](https://github.com/google/ink-stroke-modeler) documents a comparable C++ smoothing/prediction API. Similar identifiers and parameters make it a candidate for independent evaluation; the captured evidence does not establish the provenance or exact revision of Drawboard's module. No dependency was added.

### Official workflow corroboration

Drawboard documents [partial versus whole erasing](https://www.drawboard.com/blog/how-to-use-the-pen-eraser), [pressure-sensitive and hold-to-recognise ink](https://support.drawboard.com/hc/en-us/articles/6678832900751-Advanced-ink), and [touch annotation settings](https://support.drawboard.com/hc/en-us/articles/4406251851791--Windows-Touch-to-Annotate). These describe options and gestures, not measured smoothing quality. Drawboard Projects documentation was excluded from PDF-engine conclusions because it describes a different product.

### UTN comparison

UTN's `Stroke::intersectWithEraserDisc` computes intervals where the widened centreline touches a disc. `ErasableStroke` retains the complementary path sections and clones ordinary strokes. `StrokeView` redraws those remaining strokes with their caps and width/pressure. This representation removes whole cross-sections of thick ink; improving the interval maths alone cannot represent a small internal hole.

The diagnostic `utn-eraser-visual-probe` calls those actual UTN classes and renders original, actual result, and a visible-disc raster reference. Fixtures use a horizontal 120-unit stroke, radius 3, widths 2 and 20, and centres at y=50 or y=59. Counts exclude original low-alpha pixels and a one-device-pixel uncertainty band around the eraser boundary.

| Case | Render scale | Original solid ink pixels | Solid pixels lost outside disc |
| --- | ---: | ---: | ---: |
| Thin | 1× | 240 | 0 |
| Thick, centre | 1× | 2700 | 144 |
| Thick, edge | 1× | 2700 | 32 |
| Thin | 2× | 972 | 0 |
| Thick, centre | 2× | 10824 | 648 |
| Thick, edge | 2× | 10824 | 152 |
| Thin | 4× | 3884 | 4 |
| Thick, centre | 4× | 43368 | 2708 |
| Thick, edge | 4× | 43368 | 664 |

No retained solid interior pixels were counted in these nine cases. Pixel counts are resolution-dependent, not timings. Cairo render scale is not a test of the Windows zoom-to-input coordinate conversion. The reference is an output oracle, not an implemented persistent eraser or evidence of Drawboard's output. The current defect remains unfixed.

Reproduce with `cmake --build build --target utn-eraser-visual-probe`, then run `utn-eraser-visual-probe OUTPUT_PREFIX` from the build directory (parent output directory must exist). It writes JSON lines and original/actual/reference PNGs plus labelled comparisons. Successful exit means images were generated, not that the eraser met the reference. Windows application, diagnostic and test executable built successfully; all **271 enabled tests in 43 suites passed** after this change. The two existing disabled shape tests remain disabled.

## 2. Penly — capability and documentation pass

Android tooling was checked first through REA. No authorised APK or connected Android device was available to select; no third-party package was downloaded. No Android runtime or decompiled Penly evidence was collected.

The developer's [Google Play description](https://play.google.com/store/apps/details?id=com.penly.penly&hl=en) lists handwriting, erasing, text/image/shapes, transformations, grouping/locking, undo/redo, reusable collections, and editable/native versus PDF export. The [official tutorial page](https://www.penly.net/tutorials.html) links to its video channel. Those videos were not measured or frame-analysed here. Pressure curves, smoothing algorithms, palm rejection, highlighter blending, eraser precision and zoom latency remain unknown. Developer claims of smooth handwriting are not benchmarks.

UTN already has stroke stabilizers, pressure-dependent widths in `StrokeHandler`, separate touch handling and annotation selection. The next useful work is replaying recorded input through those existing paths and measuring endpoint error, corner preservation and added delay; not adding an Android-specific drawing layer. Reusable tool presets and reliable temporary eraser switching are interaction candidates, not recovered Penly mechanisms.

## 3. Xodo — limited runtime and object-model documentation

REA evidence `ev_bcbb603693a2733a36b93ab6be24d6f95980dfc9657320f2d88315ffc80c16ac` captured the [web editor entry](https://xodo.com/pdf-editor), its local/cloud file choices and consent dialog. No file was uploaded and no annotation manipulation was performed. Client-side processing wording was observed on the page, not independently verified by a network experiment.

Xodo's desktop documentation specifies [grouping annotations](https://feedback.xodo.com/support/solutions/articles/35000251370-grouping-annotations): a parent and children share operations, with exclusions for some annotation types. Its [stamp rotation guide](https://feedback.xodo.com/support/solutions/articles/35000251490-rotating-rubber-stamps) describes a rotation handle, constrained rotation and a property editor. These are desktop documentation findings and must not be generalized to the web product without testing.

UTN `EditSelection` already handles multiple elements, transforms, type-dependent properties, undo records and stacking order. Object Eraser already exists and uses object bounding boxes. A rectangular hit on an image can include transparent regions; that is a candidate interaction limitation, not evidence of data corruption. Persistent grouping, overlap-cycling, annotation anchors and a unified object menu need further model/UI inspection. Do not add another eraser mode.

## 4–13. Preliminary workflow references; runtime investigations remain open

These entries establish concrete documented workflows for planning. They are not completed REA investigations, validated application behaviour or internal implementation findings. Browser tools remain available for web targets; native targets require a selected accessible artifact/runtime. No authenticated accounts or native artifacts for these products were inspected.

| Application | Official evidence examined | Established documentation finding | Unresolved technical work |
| --- | --- | --- | --- |
| PDF24 | [Merge](https://tools.pdf24.org/en/merge-pdf), [Creator](https://tools.pdf24.org/en/creator) | The online merge tool processes on servers; Creator is the offline alternative. File order/page selection and separate save are explicit workflow steps. | REA navigation failed before initial capture; no PDF job ran. Native processing internals, encryption errors, compression and OCR remain untested. |
| Sejda | [Organize](https://www.sejda.com/organize-pdf) | Source markup describes page/file views, multiselection, inversion, deletion and blank-page insertion. | REA `ev_641210686ec39c830e95a62abdb184e11f1bd77e67d2ccd091ce01bfb1dbb9d0` saw only the visible upload entry. Hidden template controls are not tested operations. No PDF uploaded. |
| Foxit | [OCR manual](https://help.foxit.com/manuals/pdf-editor/mac/en-us/2026.2/Create_and_Convert_PDF_Files.html), [accessibility](https://help.foxit.com/manuals/pdf-editor/mac/en-us/2026.2/Accessibility.html) | Distinguishes searchable-image text from editable replacement text, language selection and suspect review. Reading order/tags are separate accessibility concerns. | No OCR run, coordinate measurement, word-selection test or native inspection. These are macOS manual findings. |
| Bluebeam | [Comparison guide](https://support.bluebeam.com/revu/features/compare-documents-vs-overlay-pages.html) | Comparison creates reviewable difference markups; overlay uses coloured layers. Alignment matters and output is separate from input. | No native comparison, anchor persistence or large-list performance measurement. |
| Lumin | [Comments](https://help.luminpdf.com/en/articles/12855273-how-do-i-add-and-reply-to-comments-within-a-document), [highlights](https://help.luminpdf.com/en/articles/12855146-how-do-i-highlight-text-in-a-document) | Describes comment pins/replies/resolution, a history panel, adjustable text-highlight endpoints and contextual formatting. | No authenticated editor interaction, thread persistence or object export inspected. |
| Heptabase | [Deep links](https://support.heptabase.com/en/articles/11176386-use-deep-links), [MCP reference](https://support.heptabase.com/en/articles/17060792-heptabase-mcp-tool-reference) | Links may address PDF pages and playback positions. Current MCP documentation explicitly lacks precise PDF-highlight source anchors. | No source-anchor export or card editing tested. A source-link feature alone does not prove exported coordinate completeness. |
| Glean/Genio Notes | [Playback](https://help.genio.co/article/183-playing-audio), [labels](https://help.genio.co/article/180-adding-labels) | Notes/labels anchor playback, current labels highlight during playback, and speed is adjustable. | Original glean.co URL failed to fetch; current official references are Genio. No playback timing, TTS, word alignment or offline trace tested. |
| MagicSchool | [Rubrics](https://www.magicschool.ai/tools/rubric-generator), [writing feedback](https://www.magicschool.ai/tools/writing-feedback) | Public tool descriptions expose assignment/level/objective inputs, editable rubrics and teacher review of feedback drafts. | These are product descriptions, not a tested generation session or evidence of grading accuracy. |
| Visual Paradigm Online | [Diagram workflow](https://online.visual-paradigm.com/diagrams/solutions/free-web-based-flowchart-maker/) | Describes shape-attached connectors, alignment, grouping and reusable formats. | No editor manipulation or connector-anchor storage inspected; robustness claims are unverified. |
| StoryJumper | [Narration teaching workflow](https://www.storyjumper.com/writing-project/page/narrate-a-book) | Describes page-specific voice recording/playback and reusable narrated templates. | No recording, media decoding, GIF support or PDF fallback tested. Narration does not establish animated-image support. |

## Remaining investigation gates

Drawboard: trace and execute synthetic ink/erase gestures, inspect export, then test physical pen input. Penly: obtain an authorised local APK and suitable tooling/device or reproducible owner recordings. Xodo: use a synthetic PDF to exercise mixed selection, overlap, transforms, grouping, delete/undo and export. Continue the other applications one at a time with synthetic documents and record actual states, not inferred success from landing-page controls.
