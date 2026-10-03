# UTN 0.2 — The Teacher Interface

UTN 0.1 proved the classroom features. UTN 0.2 makes them feel like one deliberate teaching application.

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
File actions, undo/redo, Clear, Student View, Presentation, Appearance.

### Left tool rail
Pen, Eraser, Markup, Text, Answer Box, Feedback, Shapes/Draw, Select, Hand.

### Context bar
Tool-specific properties. UTN 0.2 begins with the existing controls arranged here; later revisions will dynamically change this bar when tools change.

### Bottom status bar
Page, page label, layer, navigation, zoom, presentation/student-view state.

## Markup
Smart Highlighter becomes the primary markup tool. The legacy highlighter remains available in Classic layouts but is removed from the main UTN shell.

Planned markup modes:
- Freehand
- Straighten
- Snap to Word
- Snap to Line
- Underline
- Strikethrough

## Text
Text is a first-class teaching tool. Planned contextual controls:
- Font family
- Font size
- Bold / italic / underline / strikethrough
- Text colour
- Left / centre / right / justify
- Line spacing
- Lists
- Background
- Answer-box conversion
- Border, padding and corner radius

## Appearance
Supported shell appearances:
- System
- Light
- Dark
- High Contrast

These affect only application chrome, toolbars, panels, popovers, sidebars and workspace framing. PDF pages, notebook pages, images and exports are never theme-inverted.

## Clear workflow
The Clear control should prioritise safe, undoable classroom cleanup:
- Delete selection
- Clear current layer
- Later: clear page annotations across layers
- Later: clear temporary presentation ink
- Later: document-wide annotation cleanup with confirmation

## Current 0.2 implementation goals
- UTN shell layout
- UTN-specific shell styling
- shell appearance control
- High Contrast shell mode
- Clear control
- remove legacy Highlighter from the primary UTN shell
- keep Classic Xournal layouts available
- preserve all UTN 0.1 features
