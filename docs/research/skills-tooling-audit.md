# Safe skills and tooling audit

Audit date: 2026-10-19. Scope: this Codex session and the local UTN checkout. During this discovery sub-audit, no skills, dependencies, services, hooks, credentials, or global configuration were installed or changed; the later coordinating build download is recorded below. Reading skill instructions proves they load as text; it does not certify every bundled executable or prove a provider works.

Coordinating build addendum: after this discovery audit, the lead configured an isolated `build-qa` tree with the repository's existing `DOWNLOAD_GTEST` option. It downloaded official `google/googletest` v1.16.0 source for the tests, using the existing CMake recipe inspected in `test/CMakeLists.txt`; it was not installed globally. No new skill was installed. Network and Git-metadata operations needed sandbox approval. Optional cpptrace was disabled after its initial clone was blocked; GtkSourceView was also disabled for QA. See the completion report for actual compile/test outcomes. The archive recipe has no explicit SHA256 pin, so version-tag provenance is weaker than an immutable content hash; no unrelated credentials or opaque installation script were used.

## Decision

Reuse the installed development workflow skills, repository tests, MSYS2 toolchain, GitHub CLI, and connected inspection tools. No missing skill justifies another installation for the initial P0/P1 work. Native GTK correctness requires native tests and input-device validation; browser automation is not a substitute.

## Inspected skills

| Skill | Source and version evidence | Purpose / UTN use | Compatibility and security observations | Status |
|---|---|---|---|---|
| skill-installer | Bundled `~/.codex/skills/.system/skill-installer`; no version in SKILL.md; curated source `openai/skills` | Controlled discovery/install if a real gap appears | SKILL.md and installer entry script read; scripts searched for subprocess/network/token/write behavior. Downloads GitHub archives, checks extraction paths, runs Git fallback, writes skill destination and temporary files. Optional existing GH/GITHUB token is used for HTTP. Not a blanket certification of any downloaded skill. | Installed; instructions actually loaded; installer not executed |
| find-skills | `~/.agents/skills/find-skills`; version and publisher not established locally | Discovery procedure | Instructions loaded. Its suggested `npx skills` can fetch executable npm code; suggested global installation conflicts with conservative default here. No CLI executed. Popularity is discovery evidence, not a security guarantee. | Installed; instructions loaded; no package acquisition |
| Superpowers verification-before-completion | Cached plugin 7.0.0; `.codex-plugin/plugin.json` identifies Jesse Vincent / `https://github.com/obra/superpowers`, MIT | Require fresh build/test evidence | Entire skill read. Text-only verification procedure. Local Codex manifest declares empty hooks. This does not audit all files in the plugin. | Installed; loaded |
| Superpowers systematic-debugging | Same 7.0.0 manifest | Root-cause investigation for eraser/input/crash defects | Initial investigation instructions read; process guidance, not a C++ checker. Full skill must be read before applying remaining phases. | Installed; partially inspected |
| reverse-engineer-anything | `~/.agents/skills/reverse-engineer-anything`; SKILL metadata version 42; references `github.com/morluto/rea`, pinned CLI example 6.5.0 | Interpret shipped-artifact or runtime evidence when source is insufficient | Routing/setup section read. Explicitly excludes ordinary complete-source architecture work. Setup and npx acquisition can mutate configuration or cache and are unnecessary here. Connected tools are exposed, but providers were not exercised by this audit. Publisher identity beyond local references not independently authenticated. | Installed; partially inspected; no setup |
| ui-ux-pro-max | `~/.agents/skills/ui-ux-pro-max`; no version established in inspected header | Teacher toolbar, touch targets, focus, contrast guidance | Initial instructions read; Python search dependency. Web/ARIA recommendations need translation to GTK accessible names, keyboard focus, and native controls. Does not establish GTK expertise. Scripts not comprehensively audited or executed. | Installed; partially inspected |
| PDF skill | Runtime bundle `openai-primary-runtime/pdf/26.1015.12040/skills/pdf` | PDF generation/rendering QA | Initial instructions read. Existing runtime provenance and versioned cache path; PDF parser/render tools still consume untrusted files and should stay sandboxed. No installation performed. | Installed; partially inspected |

Other catalogued candidates (not load-verified by this audit) include surgical-patch, safe-refactor, migration, test-driven-development, requesting-code-review, graphify, frontend-design, browser skills, and computer-use. Presence in the session catalogue is weaker evidence than reading their instructions or exercising their tools.

## Trusted-source discovery decisions

Primary-source web discovery inspected OpenAI's public curated skill descriptions. These links are discovery evidence, not an install approval or immutable version pin.

| Candidate | Source | Finding | Decision |
|---|---|---|---|
| playwright | [OpenAI curated skill](https://github.com/openai/skills/blob/main/skills/.curated/playwright/SKILL.md) | Browser CLI workflow overlaps the 25 already exposed Playwright MCP tools; cannot test native GTK ink routing. Remote manifest/scripts not fully audited. | Do not install duplicate |
| playwright-interactive | [OpenAI curated skill](https://github.com/openai/skills/blob/main/skills/.curated/playwright-interactive/SKILL.md) | Inspected instructions require disabling sandboxing and adding npm/browser dependencies. This violates the user's automatic-install security conditions. | Reject for this task; no security setting changed |
| security-best-practices | [OpenAI curated skill](https://github.com/openai/skills/blob/main/skills/.curated/security-best-practices/SKILL.md) | Explicitly supports Python, JavaScript/TypeScript and Go. It supplies no dedicated C++ security coverage. | Do not install for native core |
| C++/GTK/Xournal++ niche skills | Official-source search did not establish a vetted candidate | No package with verified source, complete code inspection and useful nonduplicate coverage identified. | Use upstream documentation and existing architecture; do not install unverifiable packages |

The installer identifies `https://github.com/openai/skills/tree/main/skills/.curated` as its default trusted registry. Community skills.sh is a discovery index, not an authority to execute packages. Neither registry's complete inventory nor maintenance status of every package was audited.

## Toolchain and automation evidence

Commands were actually run for these versions: Git `2.55.0.windows.5`, GitHub CLI `2.102.0`, Node `v24.20.0`, Codex CLI `0.163.0`, MSYS2 CMake `4.4.3`, MSYS2 GCC `16.2.0`. Codex printed a PATH-alias/home warning; version output does not establish all CLI functions work.

PowerShell command discovery found Git, gh, Node/npm and Codex on PATH. It did not find CMake, Ninja, GCC, Python or CTest there. Explicit file inspection found `C:/msys64/mingw64/bin/{cmake,g++,ninja,python,ctest,pdftoppm}.exe`; CMake/GCC version commands succeeded. Use the existing MSYS2 environment or explicit executable paths rather than reinstalling. Tool presence is not a successful UTN build, package build, Python module import, or PDF render.

Session tool metadata exposes 207 REA tools, 25 Playwright tools, browser_use, codex_tui, node_repl and codex_apps connectors. Provider availability beyond exposed metadata was not tested; REA engines such as Ghidra must not be assumed installed. CUA's current instructions disable native computer APIs, so do not promise desktop interaction from that surface. Do not dump connector configuration or authentication tokens to audit files.

Repository evidence: `.github/workflows/utn-windows-build.yml` builds on Windows 2022 through existing MSYS2 dependency/build actions and runs `test-units`; `.github/workflows/continuous-integration.yml` provides cross-platform testing and locale variants. Existing installer/release workflows include `make-installer-windows.yml`, `utn-windows-release.yml`, `create-installers.yml` and `create-release.yml`. Their presence does not mean this branch's checks ran. UTN Windows workflow triggers `utn-dev`/`feature/**` pushes and PRs targeting `utn-dev`; branch selection must account for these triggers.

No repository `.agents` or `.codex` directory was found by the targeted file inventory. User-level skill/catalog instructions still apply. `RTK.md` was read: explicit read-only RTK usage only, automatic hooks disabled, raw fallback permitted, no permission weakening for compression.

## Coverage of all requested categories

| # | Category | Existing coverage / relevant UTN use case | Gap or validation needed |
|---|---|---|---|
| 1 | C++ / refactoring | GCC, CMake; process skills; existing typed model/controller code | No specialized C++ skill verified; compile and review real diffs |
| 2 | GTK / Windows UI | MSYS2; `test/gtk_tests/toolbar/Utn*Test.cpp` | Native focus, DPI, dark/light and device checks still required |
| 3 | Xournal++ architecture | Complete repository, docs, graphify catalogue | Read actual ownership and serialization paths; skill is not upstream architecture proof |
| 4 | CMake | Existing CMake 4.4.3, Ninja, CTest and build actions | Match configured build/cache/toolchain; do not mix runtime families |
| 5 | Windows packaging | Existing Windows installer/release workflows and windows-setup | Package creation/install/uninstall not verified here |
| 6 | Unit / integration tests | `test/CMakeLists.txt`, model/control/GTK tests, lesson fixtures | Execute change-specific suites and report output |
| 7 | UI automation | Existing GTK tests; exposed Playwright tools for web evidence | Browser tools cannot automate native GTK reliably |
| 8 | Accessibility | UI/UX instructions, GTK tests | Native accessible names, keyboard order, contrast and screen-reader QA needed |
| 9 | Performance | `test/benchmarks/fileLoadBenchmarkTest.cpp`; native diagnostics | No profiler run; measure stroke/erase latency before claims |
| 10 | Memory/resource leaks | Compiler and native test harness | Sanitizer/instrumented configuration and leak tool availability unverified |
| 11 | PDF | Installed PDF skill; pdftoppm present; existing PDF source paths | Actual imports/exports and rendered comparisons required |
| 12 | Stylus/pointer | CircularEraser, ErasableStroke, EraseHandler tests; `tools/eraser-visual-probe.cpp` | Real pressure, hover, touch, device-scale and high-rate events need hardware/runtime QA |
| 13 | GitHub PR review | gh 2.102.0; review skills catalogued | Authentication/network and PR state checked by repository audit; no PR creation/merge authorized |
| 14 | CI / Windows builds | Existing UTN and upstream CI workflows | Observe remote checks and ensure chosen branch triggers expected jobs |
| 15 | Security / dependency audit | Read installer risk points; repository dependency manifests/workflows | No vulnerability scan or complete transitive audit performed |
| 16 | Docs / research | Markdown evidence reports, source reading, web search | Keep observed evidence separate from hypotheses |
| 17 | REA | Installed version-42 instructions, 207 exposed tools | Only invoke when runtime/binary evidence is needed; no provider proof from catalogue |
| 18 | Browser / Playwright | 25 exposed Playwright tools and browser_use | Do not install duplicate CLI; native testing remains separate |
| 19 | Multi-agent development/review | Collaboration tools available; this scoped audit delegated by lead | Independent review must inspect actual final diff and tests |
| 20 | Screenshot / visual regression | Existing screenshot research; PDF renderer; eraser visual probe | No automated baseline-comparison harness or thresholds established here |

## Installation boundary and remaining limitations

No new installation means there are no newly installed skills whose load checks are outstanding. Listed existing skills distinguish loaded, partially inspected, and catalogue-only states. No comprehensive trust endorsement is made for existing third-party packages merely because they were already installed. Future acquisition requires a pinned revision, publisher/source verification, full relevant manifest/script review, explicit mutation/network/credential analysis, and load validation; anything needing administrator rights, security changes, unrelated credentials or opaque remote execution needs user approval before installation.
