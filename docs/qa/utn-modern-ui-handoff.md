# UTN modern UI: handoff for the 31 October presentation

Branch `feature/utn-modern-ui-oct31`, based on `099a95b1b`. Nothing is pushed, merged or published.

## Commits (exact)

| SHA | Subject |
| --- | --- |
| `de5817162` | feat(ui): one-click Freeze and Blank, Present state, readable exits, compact app bar |
| `88650520d` | feat(ui): uniform rail chevrons and a teal current-page frame on new profiles |
| the commit that adds this file | docs(qa): modern UI handoff and screenshots |

`git log --oneline 099a95b1b..HEAD` lists them all. The code was verified at `88650520d`; the docs commit changes no code.

## What was found first

The three remote UI branches (`feature/utn-visual-polish`, `...-review-fix`, `feature/utn-m3-reviewed-ui-candidate`) are all ancestors of `099a95b1b` (`git rev-list --count HEAD..origin/<branch>` is 0 for each). Nothing needed merging or cherry-picking: the tool rail, contextual inspector, Workspace modes, design tokens, light/dark/high-contrast themes and the M3 popovers were already in the base. This work builds on them. See `docs/design/utn-design-system.md` and `docs/design/utn-visual-implementation-report.md` for what existed.

The running baseline (screenshots in `screenshots/modern-ui/before/`) showed these real problems:

1. Freeze and Blank were two clicks deep inside the Students pill menu, and Freeze was greyed out with no explanation.
2. The Present menu never showed which overlay was on, and always offered a shouty "EXIT Spotlight / Curtain" row, even with nothing to exit.
3. The Blank cover's "Show page" button was white text on light grey (unreadable).
4. The Spotlight/Curtain banner sat over the app bar and its button was pale teal on white (unreadable).
5. The rail had mixed chevron directions, "Answer Box" was cut to "Answer B...", and the current page had a heavy red frame.

## What was implemented

Classroom controls
- **Freeze** and **Blank** are always-visible toggle buttons beside the Students pill (new toolbar item `STUDENT_QUICK`, added to the UTN Teacher, Teacher Tablet and Present layouts). They share state with the pill menu in both directions. Labels say what a click does next: `Freeze` / `Resume live`, `Blank` / `Show page`. Freeze with no Student View window shows a toast explaining what to do instead of doing nothing.
- The pill tooltip explains Teacher View vs Student View for each state ("The projector shows this screen, so students see your tools and private notes...").
- **Present** menu: entries read `Spotlight is on: choose to turn off`; the exit row is renamed `Show the whole page`, is disabled unless an overlay is on, and its tooltip names Escape. The Present button gets an accent frame while an overlay is on. The state follows every path that changes it (menu, banner, Escape, tool switch) through a tool-change listener that holds only a weak reference.
- The banner is placed below the app bar and its button is high contrast. The Blank cover's button is now a high-contrast accent button and takes keyboard focus, so Enter or Space brings the page back.

Layout and visual system
- Compact windows (under 1080 px) hide the redundant "UTN" brand text and tighten padding, so Freeze, Blank, Present and Appearance stay on one line at 1024 px.
- Rail caption width raised from 10 to 11/12 characters; Text and Answer Box chevrons follow the rail orientation like every other item.
- Default current-page frame is the UTN teal instead of red (new profiles only).
- New CSS is token based (`@utn_*`), has dark and High Contrast variants, and is appended in labelled blocks at the end of `ui/xournalpp.css`.

Not implemented (honest scope): no new tool glyphs, no change to the inspector, no sidebar auto-collapse, no new keyboard shortcuts (the buttons are Tab-reachable and have accessible names and tooltips). Slides 9-15 of the product deck could not be read (see Known issues); nothing was designed from them.

## Design-system decisions

- Keep the existing teal accent, graphite dark surfaces and `@define-color` tokens. No new colours except the existing danger red for an engaged Blank and amber for Frozen, matching the existing `utn-sv-*` pill states.
- State is never colour alone: every toggle changes its label as well as its colour.
- Idle quick buttons are outlined pills so they read as buttons in the flat toolbar; engaged Freeze is amber, engaged Blank is red with white text (6.5:1 on `#b3261e`).
- Anything that hides the app bar must leave the way back readable and focusable (Blank cover) or must not cover the controls at all (banner).
- Compact layout drops decoration before it drops a classroom control.
- Skill findings applied: `ui-ux-pro-max` priority 1-2 (contrast, visible focus, 44 px/touch sizes already handled by `utnTouch`, feedback within 100 ms, no icon-only unlabeled buttons); GTK is not a supported stack in that skill, so no stack search output was used. The accessibility checklist (names, label-in-name, focus not obscured, contrast, keyboard operable) was applied by hand to the changed controls; no separate audit report was produced.

## Skills actually used

Invoked and read: `superpowers:using-superpowers` (session start hook), `ui-ux-pro-max`, `design-system`, `desktop-gtk`, `investigate-first`, `superpowers:using-git-worktrees` (already in the isolated worktree, nothing created), `superpowers:test-driven-development`, `superpowers:verification-before-completion`, `awesome-accessibility-audit`, `awesome-design-doc`.

How much each shaped the work: `investigate-first` (baseline evidence before editing), `test-driven-development` (red then green for the three new tests and the alignment assertion), `verification-before-completion` (fresh full runs before claiming results) and the accessibility checklist did real work. `desktop-gtk` is GTK4-oriented and this project is GTK3, so only the CSS/popover/keyboard guidance transferred. `design-system` and `ui-ux-pro-max` scripts were not run (token generators and a web/mobile database; the project already has a token set). `awesome-design-doc` was used for its decision/risk framing, not to write an ADR.

Not installed: `frontend-design`. Not invoked: `superpowers:brainstorming`.

## Build and test evidence

Toolchain on this PC: MSYS2 **UCRT64** (the old `mingw64` repository no longer ships `gtksourceview4` or `qpdf`), GCC 16.2.0, CMake + Ninja, Debug.

Final verification at `88650520d` (fresh full builds, then full suites):

| Check | Result |
| --- | --- |
| `cmake --build build-ui --target xournalpp test-units test-gtk-integration` | exit 0 |
| `test-units.exe` | 423 run, **422 passed**, 1 skipped (`CrashChild.run`, a helper), 0 failed |
| `test-gtk-integration.exe` | **78/78 passed** (75 before this work, 3 new) |
| `git diff --check` | clean |

New tests, all in `test/gtk_tests/toolbar/ClassroomWorkflowTest.cpp`:
- `freezeAndBlankAreOneClickInTheAppBarAndStayInStepWithTheStudentsMenu`
- `presentMenuShowsWhichOverlayIsOnAndOnlyOffersExitWhenThereIsOne` (includes left-alignment of relabelled entries and no callback into a destroyed menu)
- `compactAppBarKeepsRoomForFreezeBlankAndPresentByHidingTheBrandText`

Red-then-green was observed for the label-alignment assertion (xalign 0.5 vs 0) and for the compact app bar test (fails with the hook disabled). The first two tests were written before the code and failed to compile until it existed; that is the only red state recorded for them.

Baseline before any change: unit 422 passed plus 1 skipped and GTK 75/75, except one unexplained run. The very first `test-units` run on the new toolchain reported 43 tests not passed, starting with `CrashHandler.failedEmergencySaveKeepsTheEarlierRecoveryFile`; three later runs were fully green. That area belongs to the reliability work and was not investigated here.

## Screenshot locations

`docs/qa/screenshots/modern-ui/before/` (baseline build of `099a95b1b`) and `docs/qa/screenshots/modern-ui/after/` (`88650520d`). All are of the real running `xournalpp.exe` with a fresh profile and a sample worksheet PDF, 1280x760 unless the name says otherwise.

| After file | Shows |
| --- | --- |
| `01-default-1280.png` | Freeze and Blank in the app bar, teal page frame, uniform chevrons, "Answer Box" caption intact |
| `02-present-menu-idle.png`, `04-present-menu-spotlight-on.png` | Present menu idle (exit disabled) and with Spotlight on |
| `03-spotlight-on-banner.png`, `05-after-exit.png` | Banner below the app bar with a readable exit; after exiting |
| `06-blank-cover-light.png`, `09-dark-blank-cover.png` | Blank cover with a readable Show page button |
| `07-freeze-without-projector-toast.png` | Freeze with no projector window |
| `08-dark.png`, `10-high-contrast.png`, `11-back-to-system.png` | Dark, High Contrast and back to Match system |
| `12-narrow-1024.png`, `13-narrow-900.png` | Compact layouts |

## Known issues

- **Interaction evidence.** Partway through, the desktop locked (screensaver and LockApp running, `SetCursorPos` failed), so physical mouse input and screen capture became unavailable. The before shots and the first Freeze capture used the real desktop; every later interaction (Blank, Freeze, Present, Spotlight, Appearance, resizes) was driven by Win32 mouse messages posted straight to the app window, and shots were taken with `PrintWindow`. That exercises the real GTK event path but is not a hardware mouse. Hardware mouse, touch and stylus remain unverified.
- **Keyboard navigation** was not exercised by hand. The new buttons are ordinary GTK buttons (Tab, Space, Enter), the Blank cover takes focus on show, and tests drive the same handlers. A screen reader was not used.
- **Canvas space is not solved.** At 1024 px the 612 pt page is still cut off on the right because the inspector (about 250 px) and the thumbnail sidebar are both open. At 900 px Appearance and Workspace are clipped on the app bar (they were also clipped in the baseline). Both need a deliberate responsive rule (for example auto-collapsing the thumbnails or inspector below about 1000 px); the existing GTK test that pins the inspector at 1024 px would have to be reconsidered, so it was left alone.
- Freeze still needs a Student View window; that rule is unchanged.
- A user's existing custom `toolbar.ini` does not contain `STUDENT_QUICK`; switching to a default UTN layout (or resetting the toolbar) adds it.
- The teal page frame changes only the default; saved profiles keep red. There is no automated test for the default colour.
- The banner position under the app bar is verified by screenshot only: the test window is not laid out, so an assertion would have passed without the fix and was removed.
- The product deck (Slides 9-15) could not be opened (Drive returned "not found", the export URL returned 401). Design followed the written requirements and `docs/design/utn-m3-approved-classroom-experience.md`.
- Not touched, per instructions: SaveJob, AutosaveJob, CrashHandler, privacy algorithms, serialization. `git diff --stat 099a95b1b..HEAD` shows none of them.

## Integration considerations

- Files that the reliability worktree could also touch and so may conflict on merge: `src/core/gui/MainWindow.cpp` (banner placement, compact helper), `src/core/gui/toolbarMenubar/ToolStudentView.cpp` and `.h`. In `ToolStudentView.cpp` only `updateState`, the new `createQuickControls`, the new `ToolStudentQuick` class and the Blank cover's button focus/class changed. The Freeze/Blank privacy logic (`dropFrozenFrameIfUnsafe`, `frozenFrameIsSafe`, `setFrozen`, `setBlanked`) is called, not modified.
- The quick buttons call `setFrozen` and `setBlanked`, so the other agent's frozen-frame blanking rules apply to them automatically.
- `ToolStudentView::createItem` still returns the pill menu button because existing tests depend on that.
- Two CSS blocks rely on specificity against the old toolbar reset (`utn-sv-quick`, High Contrast override). Keep them after the generic rules.
- No reliability code change is required by this work.

## Reproduce on another PC (Windows 10/11)

1. Install MSYS2 to `C:\msys64`, open a UCRT64 shell, run `pacman -Syuu` twice, then:
   `pacman -S --needed mingw-w64-ucrt-x86_64-{toolchain,cmake,ninja,imagemagick,gettext,poppler,gtk3,libsndfile,libzip,lua,portaudio,gtksourceview4,qpdf,gtest,adwaita-icon-theme} patch make git`
2. `git clone <repo>`, `git checkout feature/utn-modern-ui-oct31` (or fetch the local branch), verify `git log -1 --format=%h` is the SHA you were given.
3. In the UCRT64 shell from the repo root:
   `cmake -S . -B build-ui -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_GTEST=ON -DPACKAGE_ARCH=x86_64`
   `cmake --build build-ui --target xournalpp test-units test-gtk-integration -j 8`
4. Tests: `build-ui/test/test-units.exe` and `build-ui/test/test-gtk-integration.exe` (GTK tests need an unlocked interactive desktop). Expect 422 passed + 1 skipped, and 78 passed.
5. Run the app: copy `build-ui/resources-templates/toolbar.ini` to `ui/toolbar.ini` (it is not tracked; the app only finds `toolbar.ini` beside `ui/`), then start `build-ui/xournalpp.exe some.pdf` from a UCRT64 shell (or with `C:\msys64\ucrt64\bin` on `PATH`). To keep your own profile untouched, set `APPDATA`, `LOCALAPPDATA` and `XDG_CONFIG_HOME` to an empty folder first.
6. Look for: Freeze and Blank beside the Students pill; Present then Spotlight shows the banner under the app bar and the Present button highlighted; Blank shows a readable Show page button; resize to 1024 px and all four classroom controls stay visible; Appearance menu switches Dark and High Contrast.
