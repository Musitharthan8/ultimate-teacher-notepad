# UTN 0.2 — The Teacher Interface

UTN 0.1 proved the classroom features. UTN 0.2 makes them feel like one deliberate teaching application rather than a collection of additions to Xournal++.

## Product principles

1. Common classroom actions should take one tap.
2. The document is never recoloured by the application theme.
3. Primary tools choose *what* the teacher is doing.
4. Context controls choose *how* the active tool behaves.
5. Tools remember their own state.
6. Teacher-only content must have an obvious visibility state.
7. Destructive actions are undoable or confirmed.
8. Touch targets must work while standing and using a stylus.
9. Advanced controls remain available without cluttering normal teaching.
10. Student View must never expose teacher-only content.
11. Ink responsiveness has priority over secondary features.
12. Features should make teaching faster, clearer, or easier.

## Shell layout

### Top app bar
File actions, undo/redo, one-tap Clear, Student View, Presentation and Appearance.

### Left tool rail
Pen, Eraser, Markup, Text, Answer Box, Feedback, Prepare / Reveal, Teaching Tools, Select and Hand.

### Dynamic context bar
The second toolbar changes with the active teaching tool.

Implemented contexts include:
- Pen: profiles, colour, size and pressure
- Eraser: continuous size
- Markup: colour, size, opacity and smart mode
- Text: font, size, style, spacing, lists and alignment
- Answer Box: text controls, presets, fill, border and box geometry
- Shape: shape identity, colour, size and fill
- Selection: editable properties, stacking order and delete

### Bottom lesson bar
A compact Lesson Navigator replaces the old page-spinner/page-label pair in the main UTN layouts. It shows the current page and classroom label, provides previous/next navigation, quick labels, previous/next labelled-page jumps and a scrollable lesson map.

Dedicated scalable icons distinguish Answer Box, Feedback, Prepare / Reveal, Teaching Tools, Presentation and Appearance. Each icon is bundled in the light and dark variants of both icon themes.

Classic Xournal++ toolbar layouts remain available.

## Markup

Markup is UTN's primary highlighter/annotation tool. The legacy Highlighter remains available in Classic layouts.

Implemented modes:
- Freehand
- Straighten
- Snap to Word
- Snap to Line
- Underline Text
- Strikethrough Text

Word/line/underline/strikethrough snapping uses selectable PDF text geometry. Image-only/scanned PDFs still need OCR before equivalent semantic snapping is possible.

## Text

Text is a first-class teaching tool.

Implemented contextual controls:
- Font family
- Font size
- Bold / italic / underline / strikethrough
- Text colour
- Left / centre / right / justify
- Line spacing
- Bullet list
- Numbered list
- Answer Box styling

Underline, strikethrough and line spacing are stored as UTN text attributes in XOPP files. Existing Answer Boxes load their style into the context bar when edited.

## Answer Boxes

Answer Boxes are text objects with teacher-friendly presentation styling:
- independent text colour
- background colour
- border colour
- border width
- padding
- rounded corners
- Model Answer / Definition / Warning / Note presets

Style changes are reflected live while editing.

## Feedback Bank

The former stamp control is now a categorized Feedback Bank.

Current groups:
- Quick
- Writing
- Language
- Comprehension
- Custom

Feedback has its own colour and places as a one-shot text object.

## Student View

Student View is a separate, clean display window that omits teacher-only layers.

Implemented classroom controls:
- Freeze Student View
- Blank Student View
- Fullscreen Student View
- Close Student View

Freeze allows the teacher to work ahead privately while students continue seeing the frozen frame.

## Presentation cleanup

Presentation Tools includes **Clear Temporary Ink**, which immediately removes temporary laser-pen and laser-highlighter strokes from all pages without changing saved annotations or the active tool. Pending fadeout timers are cancelled with the removed ink.

**Dismiss Spotlight / Curtain** restores the full page while preserving the selected tool and temporary ink.

## Prepare / Reveal

Prepared answers use layers prefixed with `UTN Reveal`. Teachers can prepare, hide and reveal them during a lesson. Student View hides both `UTN Reveal*` and `UTN Teacher*` layers.

## Clear workflow

The primary Clear button removes annotations from the current page as one undoable operation while preserving the page/PDF background.

More granular options remain available:
- Delete Selection
- Clear Current Layer
- Clear Page Annotations

## Teaching Tools

Less frequently used drawing and STEM controls are grouped instead of permanently occupying the main rail:
- Line
- Rectangle
- Ellipse
- Arrow
- Double Arrow
- Coordinate System
- Smart Shape
- Set Square
- Compass
- Equation / TeX
- Image

## Appearance

Supported shell appearances:
- System
- Light
- Dark
- High Contrast

A separate Touch-friendly controls option enlarges targets for stylus/tablet use. These settings affect only application chrome, toolbars, panels, popovers, sidebars and workspace framing. PDF pages, notebook pages, images and exports are never theme-inverted.

## Responsiveness and performance

The main UTN layouts support desktop and tablet-oriented use. Highlighter erasure retains geometric overlap checks even for small backtracks, so split highlights are repainted correctly. Subsection bounds use the existing point vector directly instead of copying the entire stroke on every cache miss.

Ink responsiveness remains higher priority than decorative UI.

## Still to do

- final UTN iconography and visual polish
- responsive/compact handling for very narrow context bars
- true round/square/flat eraser geometry
- further eraser/highlighter profiling after real classroom testing
- richer pen engines rather than profile presets alone
- OCR for scanned PDFs
- handwriting-to-text
- packaging/installer and cross-platform release work
