---
name: dev-impl
description: Review one or more required GitHub issues, investigate the repository, propose one combined implementation plan for approval, record the approved plan on every issue, and implement all issues on a single focused feature or fix branch. Use when the user invokes `$dev-impl` or asks to plan and implement one or multiple GitHub issues through an explicit approval gate.
---

# Develop from GitHub Issues

Treat planning and implementation as two separate phases. Do not make repository changes, update
GitHub, create a branch, or begin implementation before the user explicitly approves the plan.

## Require issues

Require one or more issue numbers or GitHub issue URLs as input. If none are supplied, ask for them
and stop. Resolve numbers against the current repository, preserve the user's issue order, and
deduplicate repeated references. Verify that every issue exists. Report the status of each closed
issue; do not include a closed issue in the implementation set unless the user explicitly confirms
that it should be implemented.

## Review and plan

1. Read every complete issue, including metadata, comments, linked work, and acceptance criteria.
2. Read the repository's `AGENTS.md` files and applicable development instructions.
3. Inspect relevant source, tests, documentation, build files, packaging, and CI. Check current Git
   and GitHub state using read-only operations. Do not change files or external state.
4. Identify overlaps, conflicts, dependencies, implementation order, ambiguities, security or
   compatibility concerns, and likely validation across the complete issue set. Ask only questions
   whose answers would materially alter the plan.
5. Propose one concise implementation plan for all issues that includes:
   - the complete ordered issue set and how each issue maps to the plan;
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

1. Re-read every issue and check that each remains open and has not materially changed. If any issue
   changed in a way that invalidates the combined plan, explain the change and return to the approval
   step.
2. Add the approved combined plan to every issue as a new comment headed
   `Approved implementation plan`. Identify the full issue set in each comment and preserve every
   issue body and existing comment. Include the issue mapping, plan, tests, validation, assumptions,
   and exclusions that the user approved. These GitHub writes are authorized only by plan approval.
3. Confirm the working tree is safe. Preserve unrelated user changes. Follow the repository's branch
   and base-update rules, then create one focused branch for the entire issue set, such as
   `feature/issues-<number>-<number>-<slug>` or `fix/issues-<number>-<number>-<slug>`. Keep all
   implementations on this branch; do not create a branch per issue.
4. Implement only the approved scope. If new information requires a material scope or design change,
   pause, propose a revised plan, and obtain approval before continuing.
5. Add deterministic tests that protect meaningful behavior. Run incremental checks while working
   and the repository's complete required validation after the final change.
6. Review the diff and working tree for accidental changes. Summarize the implementation, branch,
   completed issue set, tests, validation results, and any remaining limitations. Do not push, open
   a pull request, merge, close issues, or add further GitHub comments unless the user requests that
   action or the repository instructions explicitly require it.

When reporting failures, distinguish implementation failures from environment or infrastructure
failures and provide the evidence needed for the user to decide the next step.
