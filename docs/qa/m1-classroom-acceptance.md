# M1 classroom acceptance

Use a copy of the lesson, never the only classroom original. Record the build commit, Windows version, display layout
(extended or duplicated), resolution, scaling, theme, input device, page, zoom and tool/mode for every run.

## Recording results

Use one status per case and configuration:

| Status | Meaning |
| --- | --- |
| AUTOMATED-PASS | Named automated test actually executed successfully; attach its log/XML and commit. |
| MANUAL-PASS | A teacher performed every step on the stated hardware and observed the expected result. |
| FAILED | Observed result differs; attach minimal reproduction, original/saved documents and screenshots. |
| BLOCKED | A required environment/device/action is unavailable; name it. |
| UNVERIFIED | Steps have not been performed. An implemented feature is not a pass. |

Record: `case | status | commit | configuration | actual result | evidence | issue`. Keep screenshots and exports with
the source XOPP; inspect every page. Never record a manual pass from model or pixel tests.

## Teacher sequence

Start with `test/files/utn/lesson-roundtrip.xopp`, which contains two pages, handwriting, highlighter, plain text,
an Answer Box, feedback, an image and hidden answer content. Add shapes in the steps below.

| Case | Reproducible steps | Expected result | Initial manual status |
| --- | --- | --- | --- |
| C01 Ink and precise erase | Draw thick pen and highlighter over worksheet content. At 50/100/200/400% zoom, choose the smallest precision eraser; cut holes, intersections and fast straight/curved sweeps. Move/resize the erased stroke; undo/redo ten times. Also open `test/files/utn/m1-erased-stroke.xopp`. | Only ink inside the cursor path disappears. Worksheet/background and surrounding wide ink survive. Holes follow transforms and history. | UNVERIFIED |
| C02 Object erase and selection | Object-erase a stroke, Answer Box and image separately. Undo each. Select overlapping mixed objects; move, resize, delete, undo and redo. Switch Pen → Highlight → Select → Text → Pen. | Whole chosen objects disappear; neighbours and z-order survive. Restored boxes retain styling. Tool indicators match behavior and no prior special mode leaks into the next tool. | UNVERIFIED |
| C03 Shapes | Draw line, arrow, rectangle and ellipse; repeat with Smart Shapes, including a rough square and a circle. Select, move and resize through both axes. Delete, undo/redo and reopen. | Intended geometry remains selectable and stable. Squares are not silently changed into circles. Appearance/order and arrow endpoints survive history and reload. | UNVERIFIED |
| C04 Answers activation | Open worksheet, activate Start Answers Layer ten times. Add an Answer Box on each page. Edit formatting and text; navigate while editing. Hide answers, then use ordinary ink on both pages. Reveal current page, then reveal all pages. | UI responds every time. New layers have distinct names and Answers audience. Hide applies lesson-wide. Ordinary ink goes to a visible layer; editing does not move private text into an ordinary layer. | UNVERIFIED |
| C05 Answers editing/history | On an extended second display, select a hidden Answers layer while teacher ghost editing is available. Edit a box, hide/reveal during editing, navigate, delete with the box action, undo/redo. Save while revealed, close and reopen. | Teacher can edit without disclosing hidden answers to a fresh Student View frame. Boxes retain text/style and layer ownership. All answers reopen hidden. | BLOCKED: physical second display not exercised |
| C06 Student lifecycle | Open Student View; hide/reveal Answers and add visible TeacherOnly notes. Navigate and edit ink/text. Resize the student window. Close/reopen it ten times and close the main application while it is open. | Fresh frames show ordinary and revealed Answers content only. TeacherOnly notes never appear. Notifications update the current page, windows close cleanly and reopening does not retain obsolete state. | UNVERIFIED |
| C07 Freeze and Blank | Reveal answers, Freeze, hide answers and navigate. Blank, unblank, then resume live. Repeat on another page. Click toolbar controls while frozen/blank. | Freeze intentionally retains the earlier frame, including answers revealed when captured. Blank is black even while frozen. Unblank restores Freeze until Resume. Resume shows the current filtered page. Overlay outlines do not intercept teacher input. | UNVERIFIED |
| C08 Display changes | Use extended displays; move the student window between monitors, hotplug the projector, then test duplicate/same-screen mode. Repeat Freeze/Blank and close/reopen. | State/ghosting matches actual placement. Same-screen mode cannot expose faint teacher-only answers. Blank cover has a working Show page action. No orphan window or input-blocking overlay remains. | BLOCKED: monitor/hotplug validation unavailable |
| C09 Text and IME | Create plain text, feedback and Answer Boxes under and over ink. Enter Unicode, combining marks, RTL and native IME composition. Test bullets/numbers, blank-item termination, wrapping, font/spacing/color; open formatting popovers during composition. Move/delete and undo/redo. | Text, formatting and stacking survive; no lost composition, duplicate characters or unexpected focus/tool transition. Literal-prefix list limitations must be recorded rather than mistaken for structured paragraph support. | BLOCKED for native IME; other interactions UNVERIFIED |
| C10 Persistence and exports | Save the completed mixed lesson to a new XOPP. Close/reopen twice. Export every page to PDF and PNG, open outputs in independent viewers and compare against the source. Repeat with hidden/revealed answers and the exact intended layer-range option. | No missing strokes, erased masks, boxes, images or changed stacking. Default export follows visible layers; visible TeacherOnly notes can appear. Explicit layer-range export can include hidden layers. Normal PDF export is not automatically Student View export. | UNVERIFIED |
| C11 Layout and touch | At 1024×768 and 1280×720, test Windows 100/150/200% scale, light/dark/high contrast, keyboard only and touch/stylus. Scroll the short-window rail to every tool. Test long labels and formatting popovers. | Tools remain reachable, focus visible, controls operable, labels/tooltips meaningful and worksheet fit stable. Palm rejection, physical latency and touch accuracy need device observations. | BLOCKED for device checks; native layout UNVERIFIED |

## Automated evidence and limits

See [the QA run report](m1-classroom-report.md) for commands and actual results. `ClassroomWorkflow` uses real Control,
MainWindow, page input, TextEditor, undo actions, save/load handlers and Student View menu/draw callbacks. It does not
simulate a physical stylus, IME composition, monitor topology, pointer hit testing through a native overlay, or file
chooser interactions. Student pixel checks call the drawing callback on the real window's drawing area; they do not
prove compositor delivery or projector pixels. Existing unit regressions cover erasure, shapes, tool modes, renderer
privacy and PDF export. Keep these claims separate from the manual cases above.

Before using an untrusted lesson in front of a class, verify the exact student window and export mode with a known
answer marker. Freeze retaining an earlier revealed frame is intentional; hide answers before capturing that frame
when it must contain no answers.
