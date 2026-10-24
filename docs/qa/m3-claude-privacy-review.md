## Review of 3ed0e6ea4 against db4170ab2: student export privacy and highlighter/GTK changes

I found no leak of TeacherOnly or hidden-answer ink on any export or print path I could trace. I did find a Teacher-copy inconsistency, a highlighter preference that only one path consults, and test gaps around print and the dialog-to-job wiring. This is an implementation review, not merge approval or M3 acceptance. I ran no builds, tests or GUI, and I changed no files.

### Skills and commands
- **Skills invoked (Skill tool):** `awesome-code-review`, `awesome-privacy-audit`, `awesome-test-gap-audit`. I did not invoke any Codex skill and did not use Codex conclusions.
- **Git, read-only:** `git log --oneline -3`, `git diff --stat db4170ab2 3ed0e6ea4`, `git diff db4170ab2 3ed0e6ea4 -- src ui`, `git diff … -- test/gtk_tests docs/qa/...`.
- **Reads and greps:**
  - Repository instructions, `docs/qa/m3-export-privacy.md` and `docs/qa/m3-claude-options-review.md` (both were truncated in my output, so I read only their opening sections).
  - `StudentExportTest.cpp`, `LayerAudience`, `DocumentView`, the Cairo and Hybrid PDF exporters, `ImageExport`, `ExportHelper`, `PdfExportJob`, `CustomExportJob`, `PrintHandler`, `SaveJob`, `PreviewJob`.
  - `Control::selectHighlighter` and `Control::selectTool`, `ToolSmartHighlighter`, `ToolUtnContextBar`, `ToolProfileSelector`, `ComboToolButton`, `ToolButton`, `UtnWidgets`, and the glade grid.
  - `grep` for callers of `ImageExport`, `XojPdfExport`, `DocumentView`, `PrintHandler` and `setSmartHighlighter*`.
- **Parent evidence, reported only:** `build-m3-logs/batch-units.xml`, `batch-gtk.xml`, the tail of `batch-gtk.log`, and file timestamps.

### Ranked findings

1. **Medium, source-verified: "Teacher copy (includes private layers)" is inconsistent across export modes.**
   - Plain PDF and image export use `DocumentView::drawPage`, which honours `isVisible()` when no filter is set (`DocumentView.cpp:84`). A hidden Answers layer is therefore omitted. The test asserts this itself: `EXPECT_EQ(content.find("HIDDEN_ANSWER_KEY"), npos)` in the explicit-teacher test.
   - Teacher progressive and explicit layer-range export use `drawLayersOfPage`, which ignores visibility when no filter is installed (`DocumentView.cpp:101-127`, `XojCairoPdfExport.cpp:169,196`). Hidden answers and hidden drafts are included there.
   - A teacher choosing "Teacher copy" for an answer key gets answers only if they are currently revealed (or if they use progressive or range mode). It isn't a leak, but the dialog label and tooltip overpromise.
   - Teacher progressive, range, PNG and SVG output are untested.

2. **Medium (test gap), source-verified: print and the dialog-to-job wiring have no test.**
   - No file under `test/` references `PrintHandler`, `CustomExportJob`, `PdfExportJob` or `ImageExport`.
   - Deleting `view.setLayerFilter(utn::visibleToStudents)` at `PrintHandler.cpp:57` would fail no test. The privacy doc concedes that print is untested.
   - The GTK dialog test only checks `getExportAudience()`. Nothing proves `CustomExportJob.cpp:83`, `:114` and `:146` pass the audience through to the exporters.
   - Failure direction is safe: a dropped Teacher wiring falls back to Student.

3. **Medium, source-verified: the highlighter preference is only honoured on one path.**
   - `Control::selectTool(TOOL_HIGHLIGHTER)` still forces smart mode off (`Control.cpp:1382-1383`).
   - `Control::setToolDrawingType` also resets it (`:655`), and `ToolProfileSelector.cpp:71` selects the highlighter through `Control::selectTool`.
   - Only the rail button click reads `smartHighlighterPreferred` (`ToolSmartHighlighter.cpp:55`). The claim that highlighter preferences "survive incidental resets" is therefore true only for that path.
   - `Control::selectHighlighter` now writes the preference on every call (`Control.cpp:839`). That is harmless today, because the popover choice (`ToolSmartHighlighter.cpp:95`) and the rail click (`:56`) both pass a value they just chose. It is a hazard for any future programmatic caller.
   - Only the boolean is stored. The snap mode is not. Whether the `utn` custom element is saved across restarts is unverified.
   - The new test (`highlighterRestoresFreehandAndSemanticModeAfterSwitchingTools`) exercises only the rail path.

4. **Low-Medium, source-verified: the "%s options" accessible name goes stale on combo buttons.**
   - `bindActiveToolOptions` (`UtnWidgets.cpp`) bakes the name once, from the tooltip at bind time.
   - `ComboToolButton::setProminentIconCallback` later rewrites that tooltip when the combo selection changes, so the options button keeps the old tool name.
   - Whether an accelerator suffix gets captured in the label depends on call order, which I did not check.

5. **Low-Medium, GTK event tests are weaker than they look.**
   - The press and key events are synthetic: `g_signal_emit_by_name` with state 0.
   - Only Return is tested. Space, KP_Enter, modifier exclusion, touch and stylus input are not.
   - For the highlighter row, the "no reactivation" check is skipped (`if (tool != TOOL_HIGHLIGHTER)`).
   - These tests do not show that real GDK dispatch, or the real Space press/release sequence, behaves the same way.

6. **Low-Medium, behaviour change, not a leak.**
   - Layer-range export now applies the visibility filter in Student mode. A public layer the teacher hid is dropped even when it is named in an explicit range.
   - The CLI has no Teacher flag, and the quick "Export as PDF" (`PdfExportJob.cpp:33`) has no audience choice, so it is always Student.
   - Teachers who relied on those paths for full copies now silently get student copies.
   - Progressive page counts also change: layers outside the range or hidden from students no longer produce a page.

7. **Low, by design but worth surfacing.** `visibleToStudents` returns `isVisible()` for Everyone and Answers layers. A student export taken while answers are revealed includes them, and the dialog doesn't say so.

### Checked, no defect found
- Student default holds for plain, progressive, explicit-range and Cairo/Hybrid PDF, and for PNG and SVG. Print installs the filter.
- Progressive export no longer mutates layer visibility, and the test asserts the visibility is restored.
- The Hybrid overlay uses `exportPage`, so it inherits the filter. A page with only teacher ink gets a blank overlay, which doesn't leak.
- I found no other output path: no Lua or plugin export, and no other `DocumentView` consumer produces shared output.
- The glade `cbTeacherCopy` cell at (0,2) doesn't collide with any other widget in its grid.

### Acceptance limitations
- **Timing mismatch:** `batch-units.xml` and `batch-gtk.xml` are timestamped 09:26 and 09:27. Commit 3ed0e6ea4 is dated 10:00:40. The `file=` paths inside the XMLs point at the main checkout. I can't show these binaries were built from exactly 3ed0e6ea4.
- **What the reports say:** the XMLs list 327 unit tests and 41 GTK tests with 0 failures, disabled or skipped. They include `StudentExport` 6/6 and the three new GTK tests. This is reported evidence, not mine.
- **Backend coverage:** whether `ExportBackend::DEFAULT` actually exercised the QPDF/Hybrid path in the test build is unverified. Other build directories show `ENABLE_QPDF=ON`, but I didn't find the test build's cache.
- **Not covered:** physical print, native GUI behaviour, real input devices and screen readers, stylus hardware, and packaging.
- **Scope limits preserved:** the change protects annotation layers only, not PDF redaction. PDF metadata and outline are not sanitised. The Title is the document filename. Outline names come from the document's table of contents, and I didn't check whether they can hold teacher text.
- **Thumbnails:** `SaveJob.cpp:91` embeds a preview that includes visible TeacherOnly layers. This is acceptable only if native `.xopp` files are never treated as student-shareable, which the privacy doc says.

## Codex response and scope

Confirmed Student filtering in the reviewed paths; no final acceptance claim. The main build cache has ENABLE_QPDF:BOOL=ON, so DEFAULT in the imported-background regression exercised the configured Hybrid backend. The report's statement that no tests reference ImageExport is inaccurate: StudentExportTest directly instantiates it for PNG coverage. Physical print and CustomExportJob end-to-end invocation remain untested; static wiring review is not runtime evidence.

Teacher export deliberately preserves ordinary visible-page semantics, while explicit ranges/progressive output can include hidden layers. Clarified the checkbox to “Teacher copy (private content permitted)” and disclosed these semantics in its tooltip. Student output includes revealed answers, explicitly disclosed too. No hidden answer is included in Student output merely because an explicit layer range names it.

The stored highlighter preference survives reset calls and is restored by the teacher rail. Classic/profile/shortcut entry consistency and a disk settings round trip remain outstanding; the batch does not claim every entry path remembers smart mode. Snap mode already has its own ToolHandler settings key; the new preference is the enabled boolean. Combo option-name refresh after subtype changes, additional key variants, ATK activation and native focus/release validation remain open. Those findings do not establish an export leak.

The review used an immutable isolated checkout of 3ed0e6ea4. Tests ran before the commit was created, against the then-current source; they were not independently rerun by Claude. A fresh rebuild and full suite will also be recorded for the following menu batch. No native control was restarted.

Follow-up: the stale combo option-name finding is now corrected with a failing-then-passing ATK regression and lifetime-bound tooltip notifications. Native assistive activation remains untested. Final menu-batch source rebuild and 327-unit/42-GTK full runs passed; detailed paths and invocation evidence are in m3-skill-execution-log.md. This follow-up does not represent a second independent Claude review of the menu changes.
