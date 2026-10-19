# UTN M1 stabilisation report

19 October 2026. Branch: `feature/utn-research-reliability`. **Workstation handoff checkpoint, not a completed classroom release.** The user requested a push before continuing at home. Physical classroom sign-off and the PDF comparison below remain pending.

## Resume here

Implementation is committed through `77084aa47`; full suites passed 312 units and 29 GTK tests. A visual preview was misread as missing PDF ruling. Direct System.Drawing bitmap sampling at (20,320) found the same blue ruling pixel (64,160,255) in `masked.png`, `masked-pdf-render.png`, `final-masked.png` and `final-masked-pdf-render.png`. Therefore **do not treat PDF ruling loss as a proven defect**. Next compare the new PDF regression and rendered pixels with the original SOURCE operator; retain or revert only the operator change based on evidence. Preserve the added export regression. No code reversal was performed before this checkpoint.

Then finish independent review/report wording, rerun affected checks if code changes, and complete native classroom testing. The build and logs below are local and ignored by Git; source, regression tests and the manual XOPP fixture are committed. Fetch this branch on the home PC to resume.

## Verified starting state

Started at `ceeeb0fc0` after `5b9238b9a`. Fetched remote state and inspected open PRs: PR #19 remained draft/open at `f802b7b28ab745a5f82885214cc34be64d28f9ec`, with no newer relevant changes to import. The latest inspected Windows CI run was successful (39021830380). No branch was merged, pushed or published. The existing branch tracks `origin/feature/utn-classroom-ui-recovery`; this work continues locally on the verified development branch.

Existing `install/`, `build/`, `build-qa` application/package outputs were preserved. Focused regression builds reused the QA test tree early in the investigation; new application and portable outputs use `build-m1`. The only initial untracked work was `install/`. A Settings test's generated `non-existing-file-path` was moved into the ignored M1 log directory.

Read the historical [completion report](completion-report.md) and its research, feature matrix, architecture, roadmap, tooling and validation references. Supplied Kami observations remain evidence, with their documented gaps; no proprietary implementation or assets were used. No new skill or dependency was installed.

## Completed changes and source references

| Area | Change | Commit / detail |
|---|---|---|
| Shapes | Reject rough squares that fit a square substantially better than a circle; repair both disabled regressions; add rotation/size and shape model lifecycle coverage | `2d75321f9`; [shape investigation](m1-shapes.md) |
| Text | Preserve the text object's layer index during editor finalization and undo/redo, preventing an edit from raising text above overlapping annotations | `eb61811ad`; [text/UI audit](m1-text-ui.md) |
| Partial eraser | Swept circular masks preserve wide ink outside the cursor, support transforms and exact fast paths, keep history order, correct highlighter/square-cap bounds and respect existing holes | `5c2872e75`; [eraser design and limits](m1-eraser.md) |
| Persistence | Validated vector/mask metadata plus a bounded standard image fallback; old/new clipboard, malformed metadata, old-reader image transforms and overflow-save tests | `5c2872e75`; [eraser design](m1-eraser.md) |
| Answers / Student View | Trace prior lock/input-window fixes; add twenty hide/reveal renderer cycles and save/reload privacy pixels | `5c2872e75`; [audience investigation](m1-audience.md) |
| PDF export, pending comparison | Add production export plus Poppler pixel checks; direct masked ink uses OVER, but its necessity is unproven after direct ruling-pixel verification | `77084aa47` |

Principal source files: `control/tools/EraseHandler.*`, `model/Stroke.*`, `model/eraser/StrokeErasure.*`, `model/eraser/MaskedStrokeCodec.*`, `view/StrokeView.cpp`, `undo/EraseUndoAction.*`, XOPP SaveHandler/LoadHandler/XmlParser/DocumentBuilderInterface, `control/shaperecognizer/CircleRecognizer.cpp`, `control/tools/TextEditor.cpp` and `undo/TextBoxUndoAction.*` (under `src/core`). New tests cover those behaviors in the existing unit target; LayerAudience and EraseHandler regressions were extended. The four implementation commits provide the exact changed-file lists.

## Actual automated results

MSYS2 MINGW64 GCC 16.2.0, CMake 4.4.3, Ninja; existing pinned GoogleTest source reused from `build-qa`. Debug, GTest enabled, optional cpptrace/GtkSourceView disabled. PATH changes were process-local.

| Check | Result | Evidence |
|---|---|---|
| Repeated baseline | 282/282 units, 29/29 GTK passed; two existing shape cases disabled | `build-m1-logs/baseline-{units,gtk}.log` |
| Shape / text / wide-erasure RED cases | Reproduced failures before their fixes | `shapes-red.log`, `text-red-shapes-green.log`, `partial-red.log` |
| Reflected masked selection preview RED | Reproduced `zoom > 0.0` assertion; corrected transform gate | `reflection-red.log` |
| Full final unit suite | **312/312 passed**, 49 suites, exit 0, zero disabled/skipped | `units-final.log`, `units.xml` |
| Full final GTK suite | **29/29 passed**, six suites, exit 0, zero skipped | `gtk-final.log`, `gtk.xml` |
| Native application / test build | Passed | `pdf-final-build.log`, `reflection-green-build.log` |
| Full resources, translations, wrapper | Passed, including post-commit metadata rebuild | `all-build.log`, `package-build.log` |
| Diff whitespace | `git diff --check` passed before implementation commits | Tool output |

All bare log filenames above are under ignored `build-m1-logs/`. The 30-unit increase is **28 new tests plus two re-enabled tests**: two new CircleRecognizer, one TextReliability, two LayerAudience, eight PartialErasePrecision, three StrokeErasureGeometry and twelve MaskedStrokePersistence. GTK totals are unchanged. Older partial-erasing topology assertions now check actual ink and history; they were not disabled.

The 1,024-sample curved-gesture smoke test measured 2,558 ms with repeated vector clips and 53 ms after the image alpha-mask path in a subsequent local Debug run. This includes synthetic input and one render, with a no-op redraw scheduler; it does not measure physical stylus latency. Current raw timing is recorded as a property in `units.xml`.

Existing MinGW locale, deprecated API, conversion and CMake discovery warnings remain. No warning-free or full CTest claim is made; test binaries were executed directly. No sanitizer, leak detector or vulnerability scan ran. Installed ASan/UBSan/Valgrind/DrMemory runtimes were not found in the inspected MINGW64 tool directory. No physical stylus, touchscreen, projector, monitor hotplug or palm-rejection validation occurred.

## Portable package and exports

Local checkpoint ZIP: `build-m1/utn-release/UTN-0.2.1-beta-Windows-x64-Portable.zip`, 99,204,657 bytes, built from `77084aa47306467c37640eaa0bb9c8b4f41b5a09`. SHA256 independently matches the manifest: `820b57fa3285a0a43e6e7c0d996b4d00c0f7e0c7e033f04ef437e020454bc2fb`. Executable: `build-m1/utn-dist.QgHK19/bin/UTN.exe`. This unsigned Debug bundle and logs are not pushed to Git. Version output identifies `77084aa4-dirty`; the package's SOURCE-COMMIT.txt records the full hash above.

The wrapper's version invocation, masked PDF/PNG exports, two-page lesson PDF/PNG exports, and direct executable version invocation all returned waited exit 0 with only Windows system directories on PATH. Final PDFs rendered through Poppler with exit 0; lesson text includes the intended text/Answer Box and excludes HIDDEN ANSWER 42. Direct ruling pixels are present in both earlier and later masked exports. Evidence: `final-packaged-*.{log,err}`, `final-direct-version.log`, `final-*-pdf-info.log`, `final-lesson-pdf-text.txt`, `final-masked-pdf-render.png`, and `packaging-final.log` under `build-m1-logs`.

The earlier candidate remains in `build-m1/utn-release-before-pdf-fix` (the folder name reflects the initial, subsequently unsupported diagnosis). Existing staging directories and older build-qa outputs remain preserved.

## Classroom UI proposal

The source already has a scrolling tool rail, a 208-pixel contextual panel, responsive label hiding at 1080/1120-pixel thresholds, light/dark/high-contrast rules, tooltips and accessible names. These are implemented features, not measured native layout results.

The independent compact proposal retains UTN's shell: Tool labels Automatic/Show/Hide, a collapsible context panel with essential ink controls always visible, a scroll cue, and clear Finish editing / Delete box actions. Preserve worksheet fit, shortcuts and IME focus when panels change. No large UI rewrite was implemented. Acceptance gates are 1024x768 and 1280x720, 100/150/200% scale, long translations, keyboard-only use and native theme/contrast checks. See [the detailed proposal](m1-text-ui.md).

## Remaining issues and next priorities

1. Complete the native manual workflow below on the actual classroom tablet and second display. Model/render/GTK-widget tests do not prove live selection handles, repeated Control activation, timers, popovers or IME focus.
2. Student View fresh renders exclude hidden answers and TeacherOnly content. Freeze intentionally retains the earlier displayed frame; hiding answers after freezing does not redact that retained frame. Blank overrides Freeze. Ordinary PDF export uses visible layers, and explicit layer-range export may intentionally include hidden layers; it is not universally Student View export.
3. Beyond editable metadata limits, or after an older reader resaves, erased ink reopens as an image. Stroke-level editing is lost; highlighter OVER fallback can differ over coloured content. See [compatibility limits](m1-eraser.md).
4. Selection bounds conservatively include holes. Whole-stroke masked visibility uses a finite-resolution local check; existing unmasked dashed-gap/filled-interior/non-round-cap hit approximations remain. Profile longer curved gestures on the real device.
5. List formatting still uses literal prefixes: mixed bullet/number conversion and selected-paragraph behavior need a separate paragraph-model design. No new P0/P1 was reproduced beyond the fixed stacking issue. Complete multilingual/RTL/IME and popover-focus checks.
6. Run a Release/signed clean-machine install/upgrade/uninstall gate and memory/leak tooling before broad release. No NSIS installer or clean Windows VM was tested here. Clang-format CI was not executed locally; no formatter was installed.

## Manual test instructions

1. Extract the new portable ZIP into a fresh directory and run `bin/UTN.exe`. Open a copy of `test/files/utn/lesson-roundtrip.xopp`; confirm new ink is visible and answers start hidden on both pages.
2. Open `test/files/utn/m1-erased-stroke.xopp`. Confirm the gap near (100,50). Erase a thick pen/highlighter at the smallest size, intersections and fast drags at 50/100/200/400% zoom. Move/rotate/resize the ink, try whole-stroke erasing inside a hole, and undo/redo repeatedly.
3. Draw line, arrow, rectangle and ellipse. Select each, move and resize through both axes, delete, undo and redo. Try overlapping objects and shapes near page edges.
4. Create plain text, Feedback and Answer Box under/over ink. Check bullets/numbers, blank-item termination, font/spacing/colour, Unicode, deletion, moving/wrapping and undo/redo. Switch formatting popovers/tools while editing and verify keyboard/IME focus.
5. Repeatedly activate Answers, hide/reveal, navigate both pages, edit and save. Reopen and confirm answers are hidden. Check live Student View, Freeze, Blank and the exact export mode on the second display.
6. Save to a new XOPP, close/reopen, export PDF/PNG and inspect each page for the same ink, masks, text, images and intended visibility. Keep originals and attachments together.
7. Check short/narrow windows, touch controls, dark/light/high contrast and 100/150/200% Windows scaling. Record viewport, tool/mode, zoom and exact reproduction steps for failures.
