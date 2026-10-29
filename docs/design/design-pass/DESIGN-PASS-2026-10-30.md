# Teacher rail design pass (2026-10-30)

Base: `059d58c1c` (Paper & Ink `68ea23228` plus the verified presentation-exit privacy test). Native GTK3 only; no new UI framework.

## Problems seen in the running app

Captured from the installed UCRT64 build at 900, 1024 and 1280 px (`before/`):

1. The options chevron of every split tool (Select, Pen, Highlighter, Eraser, Shapes, Text) was a white pill with a shadow laid over the tool icon's top-right corner. It clipped the dashed Select rectangle, the highlighter nib and the eraser.
2. The same marker was positioned by five copies of the same four `gtk_widget_set_*` calls, so tools could drift apart.
3. The ink colour tile was a flat square, unlike every colour swatch elsewhere (circles), and did not read as "colour".

## Decisions implemented

| Principle | Decision | Where |
| --- | --- | --- |
| Consistency | One helper places every rail options marker: lower-right corner, 3 px inset | `utn::placeOptionsCorner` in `UtnWidgets.{h,cpp}`; used by `ToolButton`, `ComboToolButton`, `DrawingTypeComboToolButton`, `ToolSmartHighlighter`, `ToolTextMode` |
| Optical alignment | The marker sits where no tool glyph is drawn, so the icon stays centred and unclipped | same helper |
| Visual hierarchy | The marker has no fill or shadow at rest; it gains the accent soft fill on hover or when its tool is selected | `ui/xournalpp.css`, "Design pass 2026-10-30" block (light and dark) |
| Hit area | Marker target 16 px (previously 16 px but on top of the icon). The tool tile stays the full 38 px target, and clicking the active tool still opens its options (`bindActiveToolOptions`), so the marker is a secondary route | CSS + existing behaviour |
| Colour language | The rail colour tile is a circle, matching the swatches in the properties card | `ToolMenuHandler.cpp` (`ColorIcon::newGtkImage(..., true)`) |

No annotation, classroom or Student View logic changed.

## Verification

See the report in the commit message and `after/`. Not verified: dark theme capture (the forced-dark profile did not apply), keyboard traversal, DPI scaling, stylus input, 1280 touch mode.

## Next priorities

1. The rail scrolls: at 900x700 the Present/Reveal tools are below the fold. Group tools with separators and consider a pinned classroom group.
2. Show short tool names beside or under icons for first-time teachers (the Automatic/Show/Hide label setting exists; default is Hide).
3. A visible keyboard-focus ring on rail tiles and the options marker in all three themes.
