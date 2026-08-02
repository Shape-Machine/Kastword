---
name: dev-review
description: Review Kastword code through its KDE/Qt, audio and speech, security and privacy, accessibility and localization, build and packaging, and reliability expert perspectives; present prioritized numbered findings; wait for the user to select findings; implement only those selections; run all required validation; and commit the fixes. Use when the user invokes `$dev-review` or requests an expert, multi-perspective project code review with selectable fixes.
---

# Expert Project Review

Keep review and implementation as separate phases. During review, do not edit source files, create a
branch, commit, push, or change external state. Read-only diagnostics and local builds are allowed.

Read [references/personas.md](references/personas.md) completely before reviewing. Apply every
persona to the same review target, then consolidate overlapping observations into single findings.

## Establish the review target

Prefer a user-specified pull request, branch, commit, range, or working-tree diff. Otherwise review:

1. current uncommitted changes, if any;
2. the current branch against its merge base with `origin/main`, when not on `main`;
3. the entire repository, when on a clean `main` branch.

State the resolved target. Read all applicable `AGENTS.md` files, repository instructions, related
issues or pull-request context, and the relevant code, tests, build files, packaging, documentation,
and CI. Preserve the initial Git status and diff so pre-existing user work can be distinguished from
later fixes.

## Produce findings

Verify each finding against the actual code and avoid speculative style preferences. Use these
priorities:

- `P0`: immediate security, privacy, or data-loss emergency.
- `P1`: serious correctness, security, crash, or core-feature failure.
- `P2`: meaningful reliability, performance, accessibility, UX, packaging, or test defect.
- `P3`: concrete maintainability or quality defect with limited present impact.

List findings in one continuous numerical sequence ordered by priority, then confidence and impact.
Use this shape:

`1. [P1] Short title — Persona name(s)`

For each finding, provide precise file and line references, evidence or a reproducible scenario,
impact, and a concise recommended fix. Merge duplicates found by multiple personas and credit all
applicable personas. Do not invent findings to ensure every persona appears. If no actionable defect
exists, say so explicitly.

After the list, ask which finding numbers to implement. Accept explicit numbers, ranges, `all`, or
`none`. Stop and wait for the user's answer. Do not treat the original review request as permission
to implement anything.

## Implement the selection

After the user selects findings:

1. Resolve the selection against the original numbered list. Clarify ambiguous or invalid numbers.
   If `none` is selected, make no changes and report that no commit was created.
2. Recheck Git status and ensure the reviewed code has not materially changed. Revalidate selected
   findings; if they are stale or require a materially different fix, explain why and ask before
   proceeding.
3. Never commit directly to `main`. Use the reviewed feature/fix branch when appropriate, or create
   a focused `fix/review-<slug>` branch. Preserve all pre-existing user changes and stage only the
   selected fixes. If clean separation is impossible, pause for direction.
4. Implement only selected findings and their strictly necessary supporting changes. Add
   deterministic regression tests for meaningful behavior. Do not quietly fix unselected findings.
5. Run focused tests while developing. Inspect the Makefile, CI, and repository instructions, then
   run every applicable lint, test, sanitizer, coverage, packaging/install-smoke, localization, and
   code-quality check. Always run the repository's complete required validation target last.
6. If a required check fails, diagnose and fix failures caused by the selection. Distinguish
   infrastructure failures from code failures. Do not claim success or commit until all required
   checks pass; ask the user how to proceed if the environment prevents completion.
7. Review the final diff for accidental or unselected changes. Commit the selected fixes with a
   concise imperative message. Do not push, open a pull request, merge, or update external systems
   unless separately requested.

## Report the result

Give one consolidated summary containing:

- implemented finding numbers and their fixes;
- intentionally unimplemented findings;
- tests added or changed;
- every validation command and result;
- branch and commit hash;
- remaining limitations or risks.
