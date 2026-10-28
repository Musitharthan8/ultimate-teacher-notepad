# Paper-and-ink implementation checkpoint — `206d1459302622600fa4c0d286bbc1d06a5f2d08`

These files document the actual standalone GTK application at 1280x800, launched from the isolated UCRT64 install with a seeded worksheet and separate test profiles. They are not HTML mockups.

- `ACTUAL-GTK-BUILD-206d1459-light-workspace-1280.png` — light workspace.
- `ACTUAL-GTK-BUILD-206d1459-1024.png` — 1024 px wide, light.
- `ACTUAL-GTK-BUILD-206d1459-900.png` — 900 px wide, light.
- `ACTUAL-GTK-BUILD-206d1459-dark-workspace-1280.png` — dark workspace.
- `ACTUAL-GTK-BUILD-206d1459-high-contrast-1280.png` — High Contrast workspace.
- `ACTUAL-GTK-BUILD-206d1459-present-1280.png` — `UTN Present` toolbar, state selected at launch.

The standalone images show the current application chrome and layout, including remaining legacy controls. The Pen properties, ink dock, pinned inspector and focused/dark/High Contrast card visuals are separately captured by GTK integration fixtures in this directory's `206d1459/` subfolder; those fixtures are GTK test windows, not manually operated standalone app screenshots. In particular, the standalone app's popover was not manually opened for this capture set.

At 900 px the tool rail host is about 92 px wide and the toolbar controls remain visible; this is narrower than the previous 132 px capture but still wider than the 52–60 px concept. The 1280 image confirms the filename and clean-state check. Present styling is visible, but a state-driven presentation status banner is not implemented. The high-contrast shot retains strong outlines and readable labels, though the old mixed tool glyphs remain.

Capture environment: Windows, GTK3 UCRT64 build installed under `C:\Users\musit\utn-build-home\pi-install`, `sheet.pdf` opened with isolated profiles under `C:\Users\musit\utn-build-home\shots\paper-ink-*-profile`. Source SHA: `206d1459302622600fa4c0d286bbc1d06a5f2d08`.

Additional standalone captures after the wordmark visibility follow-up (`f8b353ed73900c814e52cb60873e1d6d43fd1ca4`): `ACTUAL-GTK-BUILD-f8b353ed-light-1280.png` and `ACTUAL-GTK-BUILD-f8b353ed-light-900.png`. The 900 image confirms the compact toolbar still fits. The GTK visibility regression passes, but the 1280 screenshot does not make the UTN wordmark visually apparent; consider that detail unresolved.
