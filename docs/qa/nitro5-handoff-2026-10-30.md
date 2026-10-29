# Handoff to nitro5 (2026-10-30, written ~02:30)

Branch: `design/utn-teacher-first-2026-10-30`. Read `AGENTS.md` first (build, test, UI conventions), then `docs/design/design-pass/DESIGN-PASS-2026-10-30.md`.

## Done on this branch (newest first)

| Commit | What |
| --- | --- |
| scroller commit (see `git log`) | App bar sits in a horizontal `GtkScrolledWindow` (`tbTop1Scroller`, `show-arrow` off). Below the width it needs, it scrolls with a visible teal scrollbar, so Appearance, Workspace, Spotlight, Pen settings and Students stay reachable at any width. Focus-into-view for rail and app bar (`scrollFocusedToolIntoView`). |
| `4df92293c` | Accessible names for Save, Open, Export, Undo, Redo and every rail options marker (`ToolButton.cpp`). New test walks rail and app bar for unnamed buttons. |
| `cbe0ef148` | 800 px app bar: "Students:" prefix dropped in compact, separators and brand slot hidden, 4 px button padding. |

## Evidence

- GTK integration 88/88 and units 425/425 (1 skipped) on the worktree that includes the scroller commit.
- Real app (Windows, UCRT64, installed build): 800x600 and 640x560 captures. At 640 the bar scrolls and Present stays reachable. Captures are in `docs/design/design-pass/after/` when present.
- Not verified: dark and High Contrast with the scroller, DPI scaling, stylus, touch, maximised window, keyboard Tab through the scrolled bar in the real app.

## Known defects and next work

1. At 800 px the app bar is about 13 px taller than before (scrollbar band or min-height in `tbTop1Scroller` CSS). Check it does not waste document space; the scrollbar should only take room when it scrolls.
2. Rail: at 800x600 only about seven tools show (rail scrolls). Check Tab/scroll discoverability and consider a visible scroll cue.
3. Rail captions default to hidden (`toolLabels = 2`, `MainWindow.h`). Decide on short captions for first-time teachers; a 60 px rail limits this.
4. Tool states: confirm hover, selected, disabled and focus are consistent for rail, app bar and menu buttons, in light, dark and High Contrast.
5. Classroom flows still to audit by click path: Spotlight, Curtain, Present exit, Student View freeze/blank, Hand tool, Undo/Redo, zoom.
6. Design tokens: the CSS has `@define-color` tokens; write down type scale, spacing and component sizes in `docs/design/utn-design-system.md`.

## Resume on nitro5

```
git fetch origin && git checkout design/utn-teacher-first-2026-10-30 && git pull --ff-only
```

Create a build dir out of tree (`cmake -G Ninja -S <repo> -B <build> -DCMAKE_BUILD_TYPE=Debug -DENABLE_GTEST=ON`), then `ninja -j2 all xournalpp test/test-gtk-integration.exe test/test-units.exe`. `all` is needed before `cmake --install` (translations). Put `C:\msys64\ucrt64\bin` on PATH to run the test binaries.

## Gotchas

- GTK test harness has no frame clock: allocations go stale after synthetic resizes. Measure preferred widths or check the real app (PowerShell + Win32 `SetWindowPos`, `CopyFromScreen`).
- Another Claude session may edit the same worktree. Check `git status` before editing and message it if so.
- `ui/xournalpp.css` has CRLF/LF history: do not rewrite the whole file.
- ECC stays on the `minimal` hook profile (user setting, not part of the repo).
