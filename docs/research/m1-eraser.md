# M1 eraser precision and persistence

19 October 2026. Independent UTN implementation; no Kami code or assets used.

## Problem and correction

Deleting centreline intervals cannot represent a small hole in thick ink: it removes the full stroke cross-section outside the circular cursor. The pre-change regression demonstrated this loss at zoom 1, 2 and 4. Each edited stroke now retains its points and a collection of swept circular capsules in document units. A fast pointer jump adds an exact capsule. Collinear consecutive capsules merge. Moving, rotating and resizing compose an affine transform onto the erased regions.

On image targets with positive diagonal scaling, StrokeView draws to a bounded alpha mask and subtracts capsules with DEST_OUT. Only that temporary mask is cleared; underlying annotations remain intact. The final colour blit retains highlighter MULTIPLY. PDF/vector targets and reflected/rotated contexts use subtractive vector clips. Separate clips preserve the union of overlapping holes. The image fast path avoids costly repeated clip intersections.

EraseUndoAction owns one original snapshot per edited stroke per gesture. The replacement keeps its layer position; final edited positions are recorded for redo. Fully covered bounds can be removed. Notifications occur after the document lock is released. Whole-stroke pressure-width behavior from 5b9238b9a is preserved. Whole-stroke clicks also check existing holes, including transformed/overlapping regions, before deleting surviving ink. This last visibility check uses a local 128-pixel alpha sample after the cheap geometric test; subpixel slivers remain a numerical limitation.

Bounds now follow the rendered fallback width for highlighters and include diagonal square caps. Empty and negative-coordinate bounds are handled safely. Masks remain conservative selection bounds: erased holes do not create independent selectable fragments.

## Save, reopen and compatibility

Ordinary strokes retain standard XOPP encoding. A masked stroke saves as a standard PNG image with optional `utn-stroke-v1` metadata. UTN validates that versioned text, restores the vector and masks, and compares the fallback transform before doing so. Missing, corrupt or unsupported metadata, or an image moved/resized by an older reader, stays an image. Erased ink is not substituted with the original unmasked stroke.

The codec uses the classic locale and max_digits10 numeric precision. It bounds text at 8 MiB, points at 100,000, masks at 4,096 and dashes at 256; rejects nonfinite coordinates, widths and transforms; and checks counts before allocating. The PNG fallback is at most 8192 pixels on an edge and 16 Mi pixels, with up to four pixels per document unit. Existing Cairo and standard C++ facilities suffice; no dependency was installed.

Exceeding metadata limits saves the clipped image rather than blocking the lesson save. After reopen that object can be moved/resized/deleted as an image; its editable stroke/pressure state and partial-stroke erasing are unavailable. A legacy reader resave can likewise discard metadata. A PNG fallback uses image OVER blending, so highlighter overlap with coloured underlying content can differ from MULTIPLY. UTN's normal vector roundtrip retains MULTIPLY. Large images may reduce fallback resolution. These are explicit compatibility limits, not claims of exact legacy rendering.

## Evidence and acceptance

- Tiny discs preserve visible ink outside the radius at zoom 1/2/4; fast sweeps cut narrow slits at wide-stroke intersections. Exact pre-change RED evidence: `build-m1-logs/partial-red.log`.
- Highlighter bounds, diagonal square caps, hole clicks, transformed/overlapping holes, round capsule ends, affine resize and overlap geometry are covered.
- Undo/redo, multi-gesture order, pressure roundtrips, ordinary XOPP, malformed/missing metadata, transformed fallback, overflow save and old/new clipboard payloads are covered. Existing centreline-fragment assertions were replaced with rendered-ink/history checks; no failing cases were disabled.
- The 1,024-sample curved gesture initially measured 2,558 ms for synthetic input plus one redraw. The alpha-mask implementation measured 53 ms in a later Debug run. These are local synthetic measurements with a no-op redraw scheduler, not device latency or a controlled hardware benchmark. Raw XML: `build-m1-logs/units.xml`.
- Review reproduced an assertion on a reflected selection-preview context (`reflection-red.log`). Restricting the image fast path to supported transforms fixes it; the regression also covers rotation.
- A visual preview initially suggested PDF ruling loss, but direct bitmap sampling confirmed ruling in both the original and later exports. That suspected defect is **not established**. Commit `77084aa47` changes direct masked ink from SOURCE to OVER and adds a passing production-export/Poppler pixel regression. Compare that regression and full rendered output against the original SOURCE path before deciding whether to retain the operator change. This check was unfinished when the user requested the Git handoff.
- Cairo vector clipping can change edge antialias coverage. Simple outside-radius tests allow two alpha levels; curved checks assert visible geometry survives. They do not assert identical edge pixels across all Cairo backends.

Full-suite, package and export results are recorded in [M1 completion](m1-completion-report.md). The manual fixture is `test/files/utn/m1-erased-stroke.xopp`: a pressure stroke has a vertical erased gap at x=100, y=50 on a 200-by-200 page.
