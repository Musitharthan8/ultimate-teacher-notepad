# UTN visual verification captures

These PNGs are GTK client-window captures produced by the optional capture path in `ClassroomWorkflow.inspectorWorkspaceModesPreserveToolStateAndPersist`. They show the current workspace at 1280 × 720 with the tool properties inspector in the light, dark, and high-contrast themes, plus rail-only and canvas-only workspace modes.

To regenerate them from the repository root on a configured Windows GTK build:

```powershell
$env:UTN_VISUAL_CAPTURE_DIR = 'docs/design/screenshots'
build/test/test-gtk-integration.exe --gtest_filter=ClassroomWorkflow.inspectorWorkspaceModesPreserveToolStateAndPersist
```

The five supplied before screenshots remain in the user's Downloads folder; they are not copied into the repository. These captures are not physical monitor captures at alternate Windows scaling settings.
