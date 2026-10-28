# UTN desktop design system

## Intent

UTN is a teacher-first annotation workspace. The worksheet stays visually dominant; application chrome groups actions and gives the current tool its settings without adding presentation-only decoration. Keep the document rendering separate from chrome styles.

## Tokens

These tokens extend the existing `ui/xournalpp.css` palette. Teal remains the interaction accent; color-coded ink swatches retain their teaching meaning.

| Token | Light | Dark | Use |
| --- | --- | --- | --- |
| Accent | `#168b7e` | `#55d6c2` | Selection, focus and active controls |
| Accent strong | `#126c63` | `#72e0cf` | Brand and readable accent text |
| App surface | `#f4f6f8` | `#20252c` | App bar and inspector |
| Raised surface | `#ffffff` | `#2a3039` | Popovers and grouped controls |
| Rail surface | `#edf1f5` | `#272e36` | Tool rail |
| Border | `#d5dce4` | `#3a424d` | Panel edges and dividers |
| Primary text | `#202833` | `#eef2f6` | Main labels |
| Secondary text | `#566273` | `#b8c1cd` | Hints and metadata |
| High contrast | Black / white / yellow / cyan focus | Same | Accessible active, focus and disabled states |

## Layout and spacing

- Tool rail: icon-first, consistent 38 px mouse target; touch target at least 48 px.
- Inspector: preferred width 250 px, constrained to 190–420 px when the window allows. At narrower widths, cap it to leave at least 520 px for the worksheet and allow the inspector to scroll.
- Inspector groups: 8 px between related controls, 16 px between sections, 1 px divider. Group titles align to the inspector's leading edge.
- Controls: consistent 30–34 px height for mouse, 44–48 px for touch. Swatches remain circular and keep their individual semantic colors.
- Focus: persistent 2 px outline with offset. High contrast uses a cyan focus outline and yellow selected state.
- Canvas: no theme chrome or inspector styling should alter page/PDF colors, zoom or layout.

## Type hierarchy

- Tool or inspector section title: GTK UI font, 1.0–1.05 em, semibold.
- Control label: GTK UI font, default application size, normal weight.
- Hint: GTK UI font, default size, secondary text color; wrap before clipping.
- Tooltip/accessibility name: full localized control name even when the rail hides visible labels.

## Component behavior

- Tool rail buttons share dimensions, icon alignment, radius and checked/hover/focus language.
- Context inspector uses a stable section rhythm across Pen, Eraser, Text, Select and classroom tools. Only actual supported options appear.
- A draggable paned separator has a visible 1 px divider and a larger interaction hit target. The document navigation sidebar remains an independent paned.
- Inspector collapse preserves its preferred width and the tool settings. Canvas-only mode is a separate persisted workspace state.
- Tool labels offer Automatic, Show and Hide. Automatic follows the existing compact-width behavior.

## Theme rules

- Light: neutral surfaces, dark readable text, subtle borders, teal active/focus states.
- Dark: graphite surfaces with distinct raised levels; avoid pure black except where a user selects high contrast.
- High contrast: black/white surfaces, yellow active selection, cyan focus and patterned/grey disabled controls. Never rely on hue alone.
- Hover, checked, focused and disabled states remain independently recognizable in each theme.

## Icon rules

Use the existing theme-aware GTK icon names and project-owned UTN glyphs. Keep toolbar glyphs visually centered and similarly sized. Preserve the current icon theme setting and accessible names; do not add unlicensed external art.

## Classroom state controls (added for the 31 October build)

- A mode that hides or alters what the class sees (Freeze, Blank, Spotlight, Curtain) is one click from the app bar and says its state in words: `Freeze` / `Resume live`, `Blank` / `Show page`, `Spotlight is on: choose to turn off`. Colour (amber Frozen, red Blank, teal overlay) is a second cue, never the only one.
- Anything that covers the app bar must leave a readable, keyboard-focused way back. Transient banners sit below the app bar, never over classroom controls.
- Compact windows (under 1080 px) drop decoration (brand text) before classroom controls. Mark such widgets with the `utn-compact-hidden` style class.
- Relabelling a menu entry with `gtk_button_set_label` centres it in GTK3; use `utn::setMenuButtonLabel` to keep entries left-aligned.
- The current-page frame uses the accent teal by default. Red is reserved for destructive or blocked states.

Evidence for these decisions is in `docs/qa/utn-modern-ui-handoff.md`.
