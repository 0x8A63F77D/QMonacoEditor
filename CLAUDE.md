# CLAUDE.md

Guidance for AI agents (and humans) working in this repository.

## Project

QMonacoEditor wraps the Monaco Editor as a native Qt widget on top of
QWebEngine. C++ API in `src/`, TypeScript frontend in `web/`, QTest suites in
`tests/`, demo app in `example/`. Specs and plans live in `docs/superpowers/`.

## Build & test

- Requires Qt 6.8+ (msvc2022_64 on Windows) with WebEngine/WebChannel, CMake
  ≥ 3.19, and npm. The CMake build runs `npm install` + `vite build`
  automatically; built frontend output lands in `resources/` (gitignored).
- Configure with `-DCMAKE_PREFIX_PATH=<Qt install>`; run tests with `ctest`
  from the build directory. Test binaries need the Qt `bin/` directory on
  `PATH` when run outside an IDE.

## Delivery flow

- One tracked task = one GitHub Issue = one branch = one PR. Small, coherent
  deliverables; no long-lived branches. Delete branches on merge.
- Every PR gets an external review: comment `@codex review` on the PR.
- Merge requires, on the **final** commit: CI green + review clean. A clean
  review means no unresolved inline threads — re-pull inline comments after
  the review posts; a bland summary body does not mean "no findings".
- Review findings challenged with evidence do not block merge, but findings
  that touch a recorded product decision escalate to the project owner.
- Self-authored files (including agent-generated configs and docs) are never
  "already reviewed" — they go through the same review loop.

## Verification rules

- Fixes are proven red-first: demonstrate the failure before the fix, and
  falsify after (revert the fix, watch the test go red, restore). A reviewer
  or verifier repeats this independently rather than trusting the report.
- Forbidden test patterns: sleeps/wall-clock waits to "stabilize", retries
  masking flakiness, weakening assertions to pass, asserting values that
  happen to equal environment defaults, and treating "the flaky test stopped
  failing" as proof of a fix.
- "Before" numbers must come from the baseline tree (clean rebuild at the
  baseline commit), never from a half-applied state.
- Test counts are accounted for item by item: after any bulk change, diff the
  before/after symbol lists and confirm the difference is exactly what was
  intended.

## Escalation ladder (count by problem, not component)

1. Second same-class finding on one problem → stop patching instances; name
   the violated invariant and restructure so the code enforces it.
2. More than 3 fix rounds on one problem → stop patching; do root-cause
   analysis.
3. Still recurring after root-cause treatment → hand the decision to the
   project owner.

## Machine-checkable vs. human-eye lanes

- **Logic/rendering correctness** (API behavior, round-trips, signals) has
  objective criteria: QTest + CI gate it.
- **Visual fidelity** (editor look, theme appearance, interaction feel) has
  no machine judge: do not iterate autonomously. Deliver one reviewable
  version; the project owner's eyes are the gate.

## Reporting

- Reports to the project owner use plain language: no unexplained
  abbreviations or self-invented labels; explain each technical term on
  first use. Lead with consequences in product terms.
- Conversation language with the current owner is Chinese; commit messages,
  code comments, and repository documents are English.
