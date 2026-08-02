---
name: dev-impl
description: Review a required GitHub issue, investigate the repository, propose an implementation plan for approval, record the approved plan on the issue, and implement it on a focused feature or fix branch. Use when the user invokes `$dev-impl` or asks to plan and implement a specific GitHub issue through an explicit approval gate.
---

# Develop from a GitHub Issue

Treat planning and implementation as two separate phases. Do not make repository changes, update
GitHub, create a branch, or begin implementation before the user explicitly approves the plan.

## Require an issue

Require an issue number or GitHub issue URL as input. If neither is supplied, ask for one and stop.
Resolve a number against the current repository. Verify that the issue exists and report if it is
closed; do not assume that a closed issue should be implemented.

## Review and plan

1. Read the complete issue, including metadata, comments, linked work, and acceptance criteria.
2. Read the repository's `AGENTS.md` files and applicable development instructions.
3. Inspect relevant source, tests, documentation, build files, packaging, and CI. Check current Git
   and GitHub state using read-only operations. Do not change files or external state.
4. Identify ambiguities, dependencies, security or compatibility concerns, and likely validation.
   Ask only questions whose answers would materially alter the plan.
5. Propose a concise implementation plan that includes:
   - the intended behavior and scope;
   - the main code or documentation changes;
   - meaningful tests or regression coverage;
   - exact validation appropriate to the repository;
   - any explicit assumptions or exclusions.
6. Ask the user to approve or revise the plan, then stop. Approval of the original request is not
   approval of the proposed plan; require a user response after presenting it.

## Continue after approval

Treat requested revisions as unapproved until the revised plan is presented and explicitly approved.
After approval:

1. Re-read the issue and check that it remains open and has not materially changed. If it changed in
   a way that invalidates the plan, explain the change and return to the approval step.
2. Add the approved plan to the issue as a new comment headed `Approved implementation plan`.
   Preserve the issue body and existing comments. Include the plan, tests, validation, assumptions,
   and exclusions that the user approved. This GitHub write is authorized only by plan approval.
3. Confirm the working tree is safe. Preserve unrelated user changes. Follow the repository's branch
   and base-update rules, then create a focused branch named from the issue, such as
   `feature/issue-<number>-<slug>` or `fix/issue-<number>-<slug>`.
4. Implement only the approved scope. If new information requires a material scope or design change,
   pause, propose a revised plan, and obtain approval before continuing.
5. Add deterministic tests that protect meaningful behavior. Run incremental checks while working
   and the repository's complete required validation after the final change.
6. Review the diff and working tree for accidental changes. Summarize the implementation, branch,
   tests, validation results, and any remaining limitations. Do not push, open a pull request, merge,
   close the issue, or add further GitHub comments unless the user requests that action or the
   repository instructions explicitly require it.

When reporting failures, distinguish implementation failures from environment or infrastructure
failures and provide the evidence needed for the user to decide the next step.
