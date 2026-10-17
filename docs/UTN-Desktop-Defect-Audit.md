# UTN desktop defect audit — 15 October 2026

The user-reported build was verified as e4ae633ff, the latest portable provided at the time. GitHub was fetched before editing; the branch had not advanced. The problems were in that build, not an obsolete download.

## Reproduced through Computer Use

- Switching Text to Eraser changed the properties panel's natural width, causing the fitted page to jump between 114% and 133% in a maximized window. A smaller window also reproduced large width/zoom changes.
- The vertical properties panel received only a short toolbar allocation, clipping its title, fourth eraser mode, size slider, font size, paragraph controls, and answer-box styling. Mouse-wheel scrolling exposed fragments while most of the sidebar remained empty.
- Object erasing an Answer Box removed its text/background but left thin red border fragments in the page cache. The bounding rectangle omitted the outside half of Cairo's centred border stroke.
- The icon rail remains visually bulky; split-tool arrows receive the same large button padding as the primary tools.

## Corrections and observed results

- Expand the contextual GtkToolItem vertically; align its content at the top. Give the properties viewport a consistent 240 logical-pixel width and ordinary scrollbars. Reset both scroll axes when rebuilding the tool controls.
- Keep colour controls compact and hints wrapped. Keep the Classroom label in horizontal toolbars and use the icon plus tooltip in the vertical rail. Give adjacent mode arrows compact hit areas.
- Include the outside half of an Answer Box's stroke in its model bounds, so partial redraw and object erasing invalidate the complete visible box.
- In the corrected desktop build, all four eraser modes and the size slider were visible; Text, Eraser and Answer Box switching retained the same 65% fitted zoom in the tested smaller window.
- In the corrected desktop build, an Answer Box disappeared completely after Object erasing, with no border fragments. Ctrl+Z restored its border, fill and text.
- Plain text was successfully removed in the original build when Object mode was explicitly selected. The user's broader text-erasing complaint was not reproduced in that test. Ink/Stroke intentionally affect handwriting, and the explanatory text now states that Object is needed for text and boxes.

## Validation and remaining work

The enabled unit suite passes, including a new raster regression that draws the real TextView at zoom 0.56, 1, 1.33 and 2, erases the object, clears only the invalidated device-pixel region, and requires zero leftover pixels. Unlike the previous element-count assertions, this catches visible cache residue.

The Save File dialog opened, but the desktop helper could not activate its modal target. After refreshing the window selection, recovery also failed ('foreground window did not report a process id'). Desktop input was stopped. GUI save/reopen, GUI PDF export, further resizing, touch/high-DPI/tablet behaviour, and the rest of the classroom workflow are not claimed as verified by this pass. Existing automated persistence tests and packaging/export checks provide narrower coverage.

This is a correction pass, not a finding that every UI defect has been eliminated. The rail still needs further visual refinement, and the thick-stroke partial-eraser precision limitation documented separately is unchanged. Test notebooks were created in separate QA instances; the user's original notebook was not edited by these eraser tests.

