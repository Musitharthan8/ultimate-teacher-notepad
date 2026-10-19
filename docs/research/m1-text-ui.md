# Text reliability and compact teacher interface

Source audit: 2026-10-19, `feature/utn-research-reliability`, starting at `ceeeb0fc0`. This is a source investigation and independent design proposal. No live classroom UI, stylus, IME, screen reader or second-display session was performed by this investigator. Existing reports' earlier test results are historical evidence, not fresh executions here. The coordinating report owns fresh build/test results.

## Evidence and current behavior

Reviewed the research directory's Kami findings/matrix, current-state audit, roadmap, tooling audit, remote-state and completion report. Kami K-TEXT-001 establishes offered text size/color controls, not working text creation, lists, formatting persistence, moving/resizing or deletion. K-UI-002 records a descriptions toggle; its pixel dimensions belong to a browser at DPR 1. Neither constitutes a native GTK design specification. Research is input data; no supplied script or external instruction was executed.

`src/core/control/tools/TextEditor.cpp` uses GtkTextBuffer plus GTK input-method callbacks and Pango layout. Enter continuation is deliberately limited to an unselected cursor at the end of a line. `ListContinuation.h` recognizes literal UTF-8 bullet-space or a positive decimal number followed by dot-space, terminates marker-only items and bounds numeric parsing to nine digits. Existing `ListContinuationTest.cpp` covers basic continuation, malformed prefixes, zero, long numbers and multibyte content. Pasted text and mid-line Enter are not structured-list operations.

The paragraph buttons rewrite the complete text buffer, not only the selected paragraph. Mixed bullet/plain lines can acquire doubled prefixes when toggled on; changing between numbering and bullets can nest textual markers. These are source-observed UX limitations, not established P1 failures. Selection-aware conversion and indentation require a separate paragraph model/UX decision rather than a speculative change in this audit.

`model/Text.cpp::cloneText` copies font, content, wrap, alignment, justification, decorations, line spacing, box style, audio and rectangular state. Pango rendering applies underline, strikethrough and line spacing. Binary serialization has the additive classroom-style extension; `control/xojfile/XmlParser.cpp` and `LoadHandler.cpp::addText` parse and apply corresponding persisted styles. This establishes implementation paths, not a fresh full save/reopen or exported glyph fidelity result.

The editor clones the original while editing. Empty existing text finalizes through `DeleteUndoAction` with the removed element's index; empty newly created text is discarded. Existing nonempty text finalization previously removed the original and appended its edited clone. `TextBoxUndoAction` also appended each swapped version. Consequently a text edit or undo could raise the object above later overlapping annotations, even though the user requested no stacking change. This is the narrow reliability issue selected for a regression.

Movement uses the editor's zoom to translate the cloned element and rejects a new bounding box outside the page. Wrapping projects drag motion onto the transformed wrap direction, rejects nonpositive widths and handles out-of-page wrapping positions. Cancel restores the model wrap value. These paths deserve rotated/scaled text and touch-device validation; their presence does not prove reliable device interaction. IM focus enters in construction and leaves in destruction. Clipboard selection and preedit callbacks exist; multilingual input, focus switching through popovers and composition cancellation remain manual gates.

## Regression and correction

`TextReliabilityTest.cpp` exercises the actual `TextBoxUndoAction` with a text object below a foreground annotation, checking object identity, stacking, content and decorations across undo/redo. An optional non-owning Document constructor fallback mirrors the existing DeleteUndoAction test seam and avoids booting Control/settings/plugins in a unit test. The normal GUI path continues to obtain the Document from Control.

The coordinator reproduced the regression before correction: the native build succeeded and `TextReliability.UndoRedoPreservesTextStackingAndFormatting` failed on element order (`build-m1-logs/text-red-shapes-green.log`). The correction now preserves the removal index when replacing an edited element, both during editor finalization and undo/redo. The missing-original finalization fallback still appends, as before. Green and full-suite outcomes belong to the coordinator's final test evidence; this investigator did not run the shared build. No file-format change or new dependency is needed. The action's page/layer/document lifetime requirements remain unchanged.

## Existing interface audit

`gui/toolbarMenubar/ToolUtnContextBar.cpp` already builds a fixed 208 px side properties panel with vertical scrolling, no horizontal scrollbar, wrapped controls and at least 160 px content height. Its horizontal variant scrolls sideways. Text controls already include a Paragraph popover for line spacing and list actions; this proposal does not treat these as absent. `ToolMenuHandler.cpp` supplies teacher hints, explicit accessible names on rail items and child widgets, horizontal icon/text orientation and specialized text-mode tools. The source's existing rail scrolling is an implemented capability, not new proposed work.

`ui/xournalpp.css` contains explicit light/dark shell styling, touch variants, active state styling and high-contrast overrides for contextual controls and swatches. There is meaningful source support for these themes; no contrast ratios, actual GTK allocations, focus visibility or screen-reader output were measured here. Existing tool state tests likewise cannot establish stylus latency or real viewport fit.

`gui/MainWindow.cpp::updateCompactLayout` already automatically hides rail labels below 1080 px and restores them above 1120 px, with hysteresis to avoid flicker; names remain in tooltips/accessibility. `ui/main.glade` already wraps the tool rail in a vertical automatic scroller. These are actual existing implementations. A manual preference below would supplement the automatic behavior, not introduce compact mode for the first time.

## Independent compact proposal (not implemented)

Retain UTN's GTK shell, teacher terminology and established icons. Offer a persistent **Tool labels: Automatic / Show / Hide** preference while retaining accessible names, tooltip hints and keyboard shortcuts. Keep Automatic as the initial default, preserving current responsive behavior. Derive its exact width from GTK preferred sizes and translated names, not a copied browser measurement.

Keep the existing context panel for text-heavy controls; add a user-controlled collapse affordance with the active tool name and a clear reopen target. Keep the active ink color, size and essential mode visible in a compact strip; place advanced typography and box decoration in the existing popover pattern. Avoid changing document fit zoom when the rail/panel presentation changes. Retain scroll reachability in short windows and show a visual cue when more controls are below the fold.

Text editing should expose one clear selected-object state with **Finish editing** and a separately labeled **Delete box** action. Preserve the distinction between deleting selected characters and deleting the whole object. Return keyboard focus predictably to the editor after a formatting choice and finalize composition before switching tools. Preserve the current separate Plain/Feedback/Answer Box modes and their colors rather than introducing another competing mode switch.

Acceptance before implementation approval: all tools/options reachable at 1024×768 and 1280×720; stable fit zoom across switches; keyboard access and meaningful accessible names in both label modes; 100/150/200% scaling; light/dark/high contrast; long translated labels; composition text and selection surviving formatting popovers; touch-sized actions without excessive worksheet obstruction. Validate visually on native GTK and the actual tablet. This document is a proposal, not a UI screenshot audit or claim of Kami parity.

## Remaining risks

Fresh GUI tests must cover ordinary text plus boxed text beneath overlapping ink: edit, move, wrap, delete, undo/redo, save/reopen and export. Unit history coverage does not prove editor finalization or rendering on a device. Add multilingual/RTL/combining text and IME composition; click overlapping text to establish the intended topmost target (the current editor searches the layer forward). Keep these as explicit follow-up gates rather than claiming a comprehensive text or accessibility certification.
