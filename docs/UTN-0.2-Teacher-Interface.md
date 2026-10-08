# UTN 0.2: The Teacher Interface

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
Save, Open and Export PDF, undo/redo, Classroom, one-tap Clear, Student View, Present and Appearance. Teacher and Tablet layouts share the same app bar.

### Left tool rail
In a fixed order: Select, Area Select, Pen, Highlight, Eraser, Text, Answer Box, Feedback, then Shapes & Lines, Hide & Reveal and Move Page. The Marking layout puts Feedback straight after the select tools. Hovering a rail button shows its name and one line on what it does.

Only one rail button is ever shown as selected: Text, Answer Box and Feedback are modes of the same Text tool, and the rail shows the one in use.

### Properties bar
The second toolbar shows the settings of the active tool, always in the same order: the tool's name, colour, size, then what is specific to that tool. It fills the toolbar row; in a very narrow window it scrolls instead of clipping.

- Colour: five classroom colours in one tap, plus More colours for anything else. Highlight has its own five highlighter colours.
- Size: XS to XL buttons for Pen, shapes, Highlight and selections.
- Pen: pressure on or off.
- Shapes: filled or outline, and Back to Pen.
- Highlight: opacity and mode.
- Eraser: continuous size (the [ and ] keys also work).
- Text: font, text size, B I U S, alignment, and a Paragraph menu for line spacing and lists.
- Answer Box: Delete Box first, the Text controls, then a Box style menu with ready-made styles and fine adjustments.
- Feedback: colour and the comment that the next click will place.
- Select: a hint until something is selected; then Delete, colour and size where they apply, Bring to front and Send to back.

### Bottom lesson bar
A compact Lesson Navigator replaces the old page-spinner/page-label pair in the main UTN layouts. It shows the current page and classroom label, provides previous/next navigation, quick labels, previous/next labelled-page jumps and a scrollable lesson map.

Dedicated scalable icons distinguish Answer Box, Feedback, Hide & Reveal, Shapes & Lines, Present and Appearance. Each icon is bundled in the light and dark variants of both icon themes.

Classic Xournal++ toolbar layouts remain available.

## Highlight

Highlight is UTN's highlighter. The rail button's arrow and the properties bar offer the same modes, with the same names:
- Freehand
- Straight line
- Snap to word
- Snap to line
- Underline
- Strikethrough

In teacher layouts Highlight never draws shapes. Shapes are a Pen mode chosen from Shapes & Lines; a shape shortcut pressed while Highlight is active switches to Pen in that shape. Classic layouts keep the upstream highlighter shapes.

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
- Model answer / Definition / Warning / Note / Marking comment styles

Answer Box styling lives in one place, the Box style menu on the properties bar.

Style changes are reflected live while editing.

## Feedback Bank

The former stamp control is now a categorized Feedback Bank.

Current groups:
- Quick
- Writing
- Language
- Comprehension
- Custom

Feedback has its own colour, set on the properties bar, and places as a one-shot text object. Enter in the Custom box places the comment.

## Student View

Student View is a separate, clean display window that omits teacher-only layers.

Implemented classroom controls:
- Freeze
- Blank screen
- Full screen
- Close

Freeze allows the teacher to work ahead privately while students continue seeing the frozen frame.

## Present

The Present menu has three sections:
- During the lesson: Temporary ink, Temporary highlight, Spotlight and Curtain.
- Tidy up: **Clear temporary ink** removes temporary strokes from all pages without changing saved work; **Show the whole page** turns Spotlight or Curtain off without changing the tool.
- Screen: Presentation mode and Full screen, each turning on or off.

## Hide & Reveal

Hidden answers use layers prefixed with `UTN Reveal`. Teachers start an answers layer, hide the answers and reveal them during a lesson. Student View hides both `UTN Reveal*` and `UTN Teacher*` layers.

## Clear workflow

The primary Clear button removes annotations from the current page as one undoable operation while preserving the page/PDF background.

Its menu offers one narrower option, Clear current layer only. Deleting a selection is done from Select.

## Shapes & Lines

Less frequently used drawing and maths tools are grouped instead of permanently occupying the main rail:
- Line
- Rectangle
- Ellipse
- Arrow
- Double Arrow
- Axes
- Smart Shape
- Set Square
- Compass
- Equation / TeX
- Image

## Appearance

Supported shell appearances:
- Match system
- Light
- Dark
- High contrast

The menu marks the appearance in use. A separate option, Larger buttons for touch and stylus, enlarges targets for interactive whiteboards and tablets. These settings affect only application chrome, toolbars, panels, popovers, sidebars and workspace framing. PDF pages, notebook pages, images and exports are never theme-inverted.

## Responsiveness and performance

The main UTN layouts support desktop and tablet-oriented use. Highlighter erasure retains geometric overlap checks even for small backtracks, so split highlights are repainted correctly. Subsection bounds use the existing point vector directly instead of copying the entire stroke on every cache miss.

Ink responsiveness remains higher priority than decorative UI.

## Still to do

- final UTN iconography and visual polish
- responsive/compact handling for very narrow context bars
- true round/square/flat eraser geometry
- further eraser/highlighter profiling after real classroom testing
- richer pen engines
- OCR for scanned PDFs
- handwriting-to-text
- packaging/installer and cross-platform release work

## Expanded classroom workflow roadmap

The approved next scope includes GIF/media support, scanned-document marking, word meanings and vocabulary cards, spelling suggestions/optional autocorrect, pasted pictures and image editing, PDF page organisation, scan cleanup/OCR, comment pins, rubrics, student marking queues and explicit student/teacher/marked exports.

These capabilities are planned. See [Classroom workflow roadmap](UTN-Classroom-Workflow-Roadmap.md) for delivery order, dependencies and acceptance criteria. Existing-tool stability, ink responsiveness and Student View privacy remain the foundation.
