# M3 initial visual and accessibility review

Scope: first M3 rail-caption/responsive-layout batch, Windows GTK, 2026-10-23. This is **not the complete redesign or a compliance certification**.

## Evidence

- `screenshots/m3/utn-visual-polish-{light,dark,high-contrast}.png`: new GDK captures of the running GTK integration executable, at 1280×720. Filenames are inherited from the existing screenshot harness; these are fresh M3 images. All three were visually inspected.
- `screenshots/m3/utn-visual-polish-{rail-only,canvas-only}.png`: fresh workspace-mode captures.
- `screenshots/m3/native-pen-{light,high-contrast}.png`: Windows Computer Use captures of the standalone development executable in `build-m3-native/bin/xournalpp.exe`. The initial standalone stage predates the final Classroom-caption and inspector reattachment corrections. It reuses an older bundle's runtime DLLs with freshly installed executable/resources; it is not a clean-package acceptance test.
- Native app exercise: select Pen, draw a red mouse stroke through Windows input, Ctrl+Z removes it; Appearance opens and switches high contrast to light, Escape closes the menu. No physical stylus, IME or screen-reader execution claimed.
- Native accessibility capture exposed the window/title/system controls only, rather than GTK toolbar children. GTK integration separately confirms ATK names on primary rail buttons. These are distinct coverage levels.

## Findings and corrections

1. Captions were entirely absent despite an Automatic/Show/Hide preference. Added captions **inside** primary button hit targets; retained the project icon set, native toggle controls and names. Fresh tests cover all 13 teacher-rail captions and active-tool preservation.
2. Initial captures ellipsized Highlight/Shapes/Area. Reserved caption width and shortened names to Shapes, Area, Reveal and Move. Full descriptions remain in tooltips. Light/dark/high-contrast captures show the resulting captions; long translations still need separate coverage.
3. Compact-mode detection used a previous allocation in `configure-event`. Moved that decision to `size-allocate`; regression exercises 1280→1024→1280 and explicit Show/Hide/Automatic.
4. The narrower, genuinely restored test window exposed an inspector reattachment timing problem. Apply saved width when reattaching and let completed allocation constrain it. Preserve the width cache during ordinary allocation so a drag is not reset before button release. The existing resize/collapse/persist GTK test is part of the full gate.

## Remaining accessibility findings (read-only audit)

- **High, Windows accessibility bridge / principle: name-role-value (4.1.2).** Computer Use could not enumerate GTK toolbar controls through Windows UI Automation. This limits native automation and is a screen-reader integration risk, not proof that every Windows screen reader fails. Next fix/verification: test the packaged GTK accessibility bridge with NVDA; ensure named GTK buttons and selected/expanded states are exposed. Do not substitute browser accessibility for this check.
- **High, contextual tool settings / keyboard and no-precision interaction (2.1.1, 2.5.8).** The small secondary arrows remain the only options entry on some tools. Implement active-icon options plus an accessible keyboard route; test Escape and focus return without another SELECT_TOOL activation.
- **Medium, rail typography / readability and text resizing (1.4.4).** Captions are small and long translated labels can ellipsize. Verify Windows 100/150/200% scale and increased system text against native allocations; wrap or shorten labels where necessary while keeping full accessible names.

UI UX Pro Max `pro-rules.md` and quick-reference priorities 1–3 were applied to these real images: consistent owned icons, visible selected state, readable labels, stable button bounds, native semantic controls, worksheet colours unchanged by chrome themes. Read-only `awesome-accessibility-audit` procedures were applied to semantic controls, names, keyboard paths, contrast/state visibility and dynamic content. No web-only ARIA or mobile safe-area rules were imposed on GTK.

Pending before delivery: active-icon popovers, Classroom menu, compact/maximized and inspector-width matrix, focus/keyboard traversal, actual screen reader, contrast measurements on composed controls, common Windows scaling, long translations, tool-specific views and reduced-motion checks. No full visual acceptance claim is made.
