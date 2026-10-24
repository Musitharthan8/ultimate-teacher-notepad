# UTN M3 — Approved Classroom Experience

**Decision:** Approved by the project owner on 2026-10-23.  
**Branch:** `feature/utn-m3-integrated-classroom`, based on M2 head `1bb8e87e6f3821c9946746792c77babf8c24ffd6`.  
**Working product name:** Ultimate Teacher Notepad (UTN); any rename requires further clearance and owner approval.  
**Status:** Product/design requirements only. THIS DOCUMENT DOES NOT MEAN THAT THE DESCRIBED UI OR FEATURES ARE IMPLEMENTED.

## North star

A beautiful, uncluttered, responsive, teacher-first annotation application: teachers can begin teaching with one tap, understand active modes immediately, recover from presentation effects without panic, and keep most screen space for the actual worksheet.

The owner explicitly approved the concept visual presented in the 23 October ChatGPT conversation: graphite upper application chrome, a compact vertical tool rail with small labels *beneath* icons, turquoise/teal active selection, a light uncluttered worksheet canvas with clear page navigation, and a contextual popover/pinnable inspector instead of permanently exposing too many controls.

The concept was a proposed layout, NOT a screenshot of executable UTN. Recreate its interaction and visual principles as an original implementation; do not attempt pixel-perfect copy of external products.

## Desktop information architecture

```text
┌────────────────────────────────────────────────────────────────────────────┐
│ UTN · Classroom Workspace            [Open] [Save] [Undo] [Redo] [Present]│
├────────────────────────────────────────────────────────────────────────────┤
│ File Edit View Navigation Journal Tools Classroom Plugins Help             │
├───────────┬────────────────────────────────────────────────────────────────┤
│ [Select]  │ English Lesson · Page 3/8                     [Zoom] [Settings]│
│  Select   │                                                                │
│ [Pen]  ◄── contextual popover appears when ACTIVE icon is tapped again      │
│   Pen     │                                                                │
│ [Highlt]  │               WORKSHEET / DOCUMENT CANVAS                      │
│ Highlight │                                                                │
│ [Eraser]  │                                                                │
│   Erase   │                                                                │
│ [Shapes]  │                                                                │
│  Shapes   │                                                                │
│ [Text]    │                                                                │
│   Text    │                                                                │
│ [Mark]    │                                                                │
│ Feedback  │                                                                │
├───────────┴────────────────────────────────────────────────────────────────┤
│ Page 3/8     Layer: Teaching Notes      Student View: Safe      Zoom 100%  │
└────────────────────────────────────────────────────────────────────────────┘
```

ASCII conveys placement only; use actual design-system tokens and GTK-reachable controls in implementation. The order and grouping of controls may adapt to window size without losing features.

### Top app chrome

- Keep normal menus File, Edit, View, Navigation, Journal, Tools, Plugins and Help; add **Classroom** as a discoverable top-level neighbour, with coherent teaching, presentation, reveal, marking, read-aloud and student-view actions.
- Keep compact quick actions for Open, Save, Export, Undo, Redo, Clear, Present, Student View and Appearance where space permits.
- Consolidate duplicated controls, but preserve keyboard bindings and Classic layout compatibility.
- Do not sacrifice worksheet height to oversized navigation bars.

### Left rail

- Compact, consistent primary icons with **short visible labels underneath**, e.g. Select, Pen, Highlight, Erase, Shapes, Text, Feedback; expand via scroll or overflow instead of silently losing tools.
- Make selection unmistakable through checked state, accent border/background, and accessibility role, not only colour.
- Mouse, keyboard, pen and touch hit targets should follow platform-appropriate sizing and the UTN design tokens.
- Avoid requiring the tiny adjacent chevron for discovering tool modes. If shown, it is secondary affordance only.
- Keep alternate input layouts (Teacher, Marking, Tablet, Classic) functional.

### Tool activation and settings

1. Tap an inactive tool icon: make that tool active immediately with its previously selected subtype.
2. Tap the **already active** tool icon: open a compact **anchored floating popover** containing subtypes and relevant settings.
3. Choose a subtype: apply it, persist it for that tool, close the popover, continue working without a second confirmation.
4. Clicking outside or Escape dismisses the popover without silently changing a drawing tool.
5. For keyboard/screen-reader users provide an equally discoverable way to open and manipulate settings; don't make rapid double-tap a requirement.
6. Optional **Pin settings** opens the full inspector. The inspector remains independently resizable/collapsible and retains prior width. Hide it by default if that maximises teaching space while preserving discoverability.
7. Open/close of the popover or inspector must not affect page rendering, current document zoom, pointer coordinates, unsaved annotations or selected pen style.

Expected tool-specific fields:
- Pen: freehand/shape types where applicable, pen colour, width, line style, pressure.
- Highlight: stroke colour, width/opacity and semantic modes without accidental arrows.
- Eraser: a safe, predictable default that removes visible editable pen/highlighter/shape ink; advanced whole-stroke vs object deletion clear and deliberate.
- Shapes: line, rectangle, ellipse, arrow, smart-shape etc.; strong icon/text hierarchy and recognizer fallback.
- Text: font, size, spacing, bold, italic, underline, strike, bullets/numbers, Enter behaviour.
- Feedback/Answer Box: comment choice, colour, content, box presentation and explicit object deletion.

### Canvas and bottom status

- Worksheet dominates. Neutral page framing; never invert or recolour document/PDF content with theme controls.
- Compact status bar indicates page, active layer, zoom and actual Student View/privacy state.
- Document navigation and tool settings must remain distinct.
- Popovers never obscure the drawing area more than necessary, and transient help must not appear repeatedly over students' work.
- Preserve Student View privacy and ensure hidden answers never leak to student display or student export.

## Initial design tokens (from existing design system; adjust using UI UX skill findings)

- Interaction accent dark theme: teal `#55d6c2`; active states must also show selection semantics.
- App chrome dark graphite `#20252c`, tool rail `#272e36`, raised panels `#2a3039`.
- Neutral white worksheet and restrained dark/light contrasting text.
- Shared spacing scale; compact but legible typography, coherent radii, focus rings and icon set.
- Theme variants: light, dark and high contrast with visible checked, hovered, disabled and keyboard-focus states.
- Preserve a responsive layout at 1024×768, 1280×720 and 1920×1080, with common Windows scaling modes. Icons must remain readable at physical laptop scale.

## Product behaviour and acceptance issues

This M3 milestone MUST integrate the existing user-reported M2 acceptance cases in `docs/feedback/utn-m2-teacher-field-test.md`. Do not erase issues from the register because CI is green.

### Non-negotiable correction set

1. **Presentation recoverability**: Spotlight and Curtain can be dismissed by reselecting the active mode, Escape, a prominent on-screen Exit/Show whole page control, and safe tool switching. A different teacher can discover the exit without instructions. Existing M2 implementation must be validated, not assumed.
2. **Ink and highlighter erasing**: test the normal eraser on pen, highlighter and shapes; define semantics for erasing across visible editable layers vs hidden/private layers; preserve deliberate object deletion and undo/redo. Never erase hidden teacher notes accidentally.
3. **Smart Shapes**: measured tests on imperfect real input plus handwriting false-positive cases; when uncertain, preserve the original drawing. Synthetic tests alone are insufficient for stylus acceptance.
4. **Tool affordances**: active-icon-tap popover and per-tool persisted subtypes.
5. **Icon plus mini label**: make every teacher tool understandable without requiring hover tooltips.
6. **Classroom main menu**: discoverable in the normal app menu bar, not stranded as an isolated action.
7. **Divider tooltip**: remove the intrusive 'Drag the divider...' teaching-surface overlay; retain accessible resizing.
8. **Text Enter**: reproduce the unusual appearance and behaviour, ensure predictable Enter / Shift+Enter and list continuation with IME/Unicode.
9. **Handwriting to font**: research and prototype a Windows/offline viable path; only mark delivered after actual recognition + editable preview/correction + undo + reload tests. If blocked, document licence/dependency and do not fake completion.
10. **Polish and discoverability**: consistent design across all tool states, not a cosmetic standalone mockup.

### Critical performance gate

A field tester observed lag followed by freeze after many consecutive stylus strokes. This is **release-blocking**, even if earlier crash fixes were merged. Reproduce on a deterministic long-stroke corpus, profile event-handler/rendering main thread, undo cache, highlighter overlaps, memory and zoom; record before/after results, and test a portable Windows build. Real hardware stylus validation remains a final owner-only acceptance item.

### Earlier feature backlog integration

Inventory, implement or accurately mark blocked/deferred the broader requested teacher tools: PDF merge/split/page editing, OCR, text formatting, handwriting conversion, feedback, read-aloud, dictionary, images/GIF/media, stamps, Answer Layers, Student View, colour picker and packaging. No fake UI placeholders. Preserve functioning XOPP, undo, PDF export and privacy.

## Mandatory skill execution, not skill name-dropping

Before implementation, **Codex must discover installed skills/MCP tools in its actual runtime, read each relevant SKILL.md, invoke it via its documented mechanism, and record evidence**. Potentially relevant installed capabilities previously used include UI UX Pro Max, Reverse Engineer Anything (REA), Arena multi-agent evaluation, native desktop Computer Use, C++/GTK debugging and visual screenshot audits. Do not assume an old install is present or a proposed MCP method exists. Observe the installed tool schemas.

- UI UX Pro Max: UX patterns, typography, contrast and native teacher workflow design.
- REA: authorised interaction research/reference only, without bypassing access or copying proprietary code.
- Arena: compare competing solutions where independent algorithms/designs can be objectively tested.
- Native GUI automation: operate a **compiled** Windows UTN; capture actual screenshots and report inaccessible controls honestly.
- C++/GTK/performance tooling: actual profiling, tests, regression isolation and Windows packaging.

Codex owns implementation and internal tests. Claude must **independently discover and invoke its applicable skills**, review code/diffs, screenshots, reproducibility and regression evidence in a separate worktree/branch. ChatGPT can inspect linked GitHub results and help review integration evidence but cannot directly operate the owner's local Windows desktop or claim Codex/Claude execution absent evidence.

Create `docs/qa/m3-skill-execution-log.md` during engineering and include for each skill: installed path, version when applicable, actual invocation, outputs, decisions applied and blockers. Mere documentation does not count as invocation.

## Owner-approved testing model

**No owner QA after every minor fix.** Codex and Claude must perform internal incremental testing, design validation and review. The owner runs a consolidated classroom/laptop/stylus acceptance check only once a major integrated M3 build has passed the internal gate.

### Major gate: M3 integrated candidate

- Complete feature-status matrix including blockers / honest deferrals.
- Green Windows CI; targeted unit, GTK, persistence, privacy and stress suites.
- Native GUI automation and screen evidence from the executable wherever accessible.
- Before/after real UI screenshots at varied sizes/themes and notes on remaining layout problems.
- High-load ink/performance report, with no known unexplained freeze.
- Independent Claude review and documented fixes.
- Verified portable build and installer if supported.
- A SHORT human acceptance plan focused on true hardware/colleague usability gaps only.
- No automatic merging or production release.

## Design acceptance walk-through (internal, not a user request)

1. Open classroom worksheet; immediately recognize rail tools without instruction.
2. Tap Pen, tap Pen again, select a different colour/width, close popover, write, switch away and back; selected mode persists.
3. Open Classroom from menu, activate Spotlight, exit with Escape or active control; repeat for Curtain, with no hidden 'show whole page' hunt.
4. Add pen, highlight, shapes and text; erase editable ink/highlighter/shape strokes predictably; delete text/Answer Boxes only deliberately.
5. Exercise Smart Shape with messy/non-shape handwriting and verify conservative conversion.
6. Compose multiline text and bullets using Enter; save/reopen and export PDF.
7. Shrink inspector, hide it, restore it, resize window and change themes; canvas remains stable and text stays accessible.
8. Run large stroke workloads and check for latency/memory/GUI freeze.
9. Verify teacher-only content is excluded from Student View and student exports.
10. Check the installed/portable final build on a clean Windows environment.

**Definition of done:** Not "the UI looks like a mockup" and not "CI is green". A teacher can operate the integrated app confidently, safely and quickly, and the internals pass a repeatable verification gate.
