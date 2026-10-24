# M3 interaction research

2026-10-23. REA 6.7.0 was used for fresh, unauthenticated Chromium 156.0.8078.47 scenarios. No proprietary code or visual assets were copied into UTN.

## Fresh evidence

1. Public features page: `https://www.kamiapp.com/features/` redirects to `https://www.kamiapp.com/products/kami-app/features/`. REA Evidence `ev_f0789ef51000c30d9d21d322984dd6a02818591d131ca1ca693ec31e048a0f0a`; one `wait_for_timeout` step, accessibility/DOM/URL capture, owned process terminated. Public copy distinguishes ink/shape erasing from clearing other annotations, identifies editable formatting and reusable feedback, and describes presentation as directing classroom focus. These are published descriptions, **not executed annotation behaviour**.
2. Fresh viewer: `https://web.kamihq.com/web/viewer.html`. Evidence `ev_fa39e6a3f1c3002a476b53a249d0dcaa50586723911098b5aab8ecf0d1c08bbe`; one wait step, accessibility/URL capture, owned process terminated. Shows login dialog and Google/Microsoft/Email buttons. Stopped at authentication; no access bypass, no annotation workflow claim.
3. Existing `kami-findings.md` records supplied 18 October screenshots/accessibility observations and their limits. Visible labels and pressed/expanded semantics were established by those historical artifacts; handwriting latency, erasing geometry and detailed text behaviour were not.

Initial scenario requests failed input validation (empty actions, then incorrect action discriminator/fields). Those requests did not execute research. Successful requests used `{action: "wait_for_timeout", step_id: "...", duration_ms: ...}`.

## UTN decisions

- Keep ink erasing and deliberate object deletion distinct and clearly named; neither the public descriptions nor historical artifacts justify sweeping deletion of hidden/private content.
- Put short names beneath primary icons while retaining native accessible names and checked states. Implement an original GTK layout with existing project icons, not copied Kami graphics.
- Repeated active-tool activation should disclose settings; keyboard focus and Escape must work through native controls. This is the owner's approved UTN interaction, not a verified claim about Kami.
- Treat reusable feedback and text editing as part of the same document/history workflow. Do not call OCR, recognition or media delivered merely because a UI command exists.
- Research cannot substitute for running UTN. Fresh native screenshots and interaction tests are still required.

REA native-UI providers report macOS-only support on this Windows host. Windows `@oai/sky` Computer Use initialized successfully as the supported native fallback. No native REA success is claimed.
