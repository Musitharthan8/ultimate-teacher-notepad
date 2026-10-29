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

## Keyboard focus (added)

Rail tiles draw their keyboard focus ring inside the tile (`outline-offset: -3px`) and the corner marker keeps a tight ring, because the rail clips outside outlines. Seen in the running app: after 18 Tab presses the Select tile shows a full 2 px teal ring (`after/` captures were not kept; the check is described here only).

## Verification

- Real-app captures (installed UCRT64 build, isolated profiles): `before/` 900 and 1024; `after/` light 900, 1024, 1280, 800x600 and dark 1280, 900. The dark captures come from a `themeVariant=forceDark` profile and show the dark chrome with the markers visible.
- GTK integration suite 85/85 and unit suite 425/425 (soak and stress excluded).
- Not verified: ring on the corner marker, DPI scaling, stylus and touch, and the Pen popover and Spotlight/Curtain overlays in the running app.

## Responsive finding (not fixed)

At 800x600 the header drops its right-hand controls (appearance toggle, two-pane view, pen) with no visible overflow cue, and the rail shows only about seven tools. Whether the menu bar still reaches those actions was not checked; either way a teacher cannot see that anything is hidden.

## Next priorities

1. Header overflow at narrow widths needs a visible "more" affordance. The rail scrolls: at 900x700 the Present/Reveal tools are below the fold. Group tools with separators and consider a pinned classroom group.
2. Show short tool names beside or under icons for first-time teachers (the Automatic/Show/Hide label setting exists; default is Hide).
3. Check the options marker focus ring and High Contrast focus on the rail.
