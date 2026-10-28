# UTN interface sprint 2: canvas-first adaptive workspace

Branch `feature/utn-ui-sprint-oct31`, based exactly on the accepted `256303cb5`.

## Problems found (screenshots in `screenshots/sprint2/before/`)
1. The tool rail, settings inspector and thumbnail sidebar are all open at once: the page got about 54% of the width at 1280 px and about 37% at 900 px, and was cut off at 1024 and 900.
2. At 900 px "Appearance" and "Workspace" were clipped from the app bar.
3. "Present" named both the mode tab and the Spotlight/Curtain menu, and six classroom controls shared one line.

## Changes
- `ResponsiveLayout.h`: pure width policy with hysteresis. The thumbnail sidebar steps aside below 1400 px (returns above 1440); the settings inspector folds to its "Show settings" handle below 960 px (returns above 1000).
- `MainWindow`: applies the policy without touching saved settings or the saved workspace mode. It never changes panels inside a size allocation (deferred to idle). A teacher's own choice (sidebar toggle, Show/Hide settings, Workspace menu) always wins and cancels the automatic restore.
- Compact app bar: Appearance, Workspace and the overlay button keep their icon, tooltip and accessible name and drop the word, so Freeze, Blank, Students and the overlay controls always fit. Workspace gained an icon.
- The Spotlight/Curtain button is now named "Spotlight & Curtain" so "Present" only means the mode tab.

## Results (screenshots in `screenshots/sprint2/after/`)
Canvas area (measured from the screenshots, window width in brackets): 1280 px: 688 px (54%) to 873 px (68%); 1024 px: about 430 px (42%) to 630 px (62%); 900 px: about 330 px (37%) to 770 px (86%). At 900 px every control stays on the app bar and the rail scrolls to Feedback, Reveal/Hide and Move (see `after-900-rail-scrolled.png`). Dark and High Contrast at 900 px were checked (`after-900-dark.png`, `after-900-highcontrast.png`).

## Tests
Unit: 3 new `ResponsiveLayout` tests. GTK: `panelsStepAsideInNarrowWindowsAndComeBackWithoutChangingSavedSettings`.

## Not verified
Physical mouse/touch/stylus, keyboard walk-through, screen reader. Other locales fall back to English for the renamed strings.
