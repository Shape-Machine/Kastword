---
name: dev-pr
description: Refresh Kastword's Plasma-themed screenshots, validate the current feature branch, create or reuse its pull request, monitor required GitHub Actions checks and a seven-minute code-review window, require user decisions on review findings and permission to merge, then rebase-merge and clean up. Use when the user invokes $dev-pr or asks Codex to open, watch, review, and merge the current feature branch. If screenshot capture or checks fail, or review feedback needs a decision, report it without merging.
---

# Create, Monitor, and Merge a Pull Request

Carry the current feature branch from final validation through a merged pull request and local
cleanup. Keep the user informed during monitoring and never merge a failing, pending, conflicted, or
draft pull request. Never merge without explicit user permission after completing the review window.

## Prepare the branch

1. Read all applicable `AGENTS.md` files and repository instructions.
2. Record the current branch, status, upstream, and recent commits. Refuse to operate on `main` or a
   detached HEAD.
3. Require a clean working tree. Do not stash, discard, or include unrelated changes.
4. Fetch and prune `origin`. Confirm the branch contains commits not in `origin/main` and inspect the
   complete proposed diff.
5. Require an active Plasma graphical session and run `make screenshots`. Confirm that
   `screenshots/` contains exactly the four expected numbered PNG files. Inspect the regenerated
   images and repository diff for accidental or private content. If tracked screenshots changed,
   stage only `screenshots/` and commit them with `Refresh application screenshots`; never create an
   empty commit. Confirm the working tree is clean afterward. Stop if capture or inspection fails.
6. Run every validation required by the repository. For Kastword, run `make validate` last. Stop if
   any required check fails.

## Create the pull request

1. Confirm GitHub CLI authentication with `gh auth status`.
2. Push the current branch to `origin`, setting its upstream when necessary. Never force-push unless
   the user separately authorizes it.
3. Reuse the branch's existing open pull request if one exists. Otherwise create one targeting
   `main` with `gh pr create`.
4. Use a concise, imperative title. Include a short summary and the exact validation commands and
   results in the description, as required by the repository instructions.
5. Report the pull-request URL to the user.

## Monitor code review

Start a seven-minute review window immediately after opening or reusing the pull request. Run this
window in parallel with required-check monitoring, poll every 30 seconds, and stop at seven minutes;
do not extend the deadline merely because checks remain pending.

At the start and on every poll, inspect all substantive feedback available through GitHub's pull
request reviews, inline review comments, and general pull request comments. Include feedback from
humans and review bots. Exclude CI status messages, empty approval or dismissal events, duplicate
records, and comments by the pull request author. Treat qualifying feedback that already exists on a
reused pull request as an immediate finding.

When review feedback is found:

1. Stop the review window and do not merge.
2. Inspect each finding against the proposed diff and relevant repository context.
3. Present a numbered list with the author, location and link when available, a concise summary, and
   a recommendation to address or ignore it with concrete reasoning.
4. Ask the user to decide what to do with the findings, then stop. Do not reply, resolve threads,
   change source, or push commits until the user directs that action.

When the seven-minute window ends without findings, ask the user for explicit permission to merge,
then stop and wait for the response. Permission does not override pending or failing checks, review
findings, conflicts, required reviews, or other repository protections. If the pull request changes
after permission is granted, restart the seven-minute window and require fresh merge permission.

## Monitor checks

Poll the pull request's required checks with `gh pr checks` or equivalent structured `gh` queries.
Use bounded waits of no more than 60 seconds so the user receives periodic progress updates. Treat
cancelled, skipped-required, timed-out, action-required, and startup failures as failures rather
than successes.

When checks are pending, continue monitoring until they reach a terminal state. Do not interpret an
unchanged pending state as a blocker and do not ask the user to keep the workflow running.

When any required check fails:

1. Do not merge or alter source code.
2. Identify the failing workflow, job, and relevant failing step.
3. Inspect failed logs with `gh run view --log-failed` and distinguish a code failure from flaky or
   external infrastructure.
4. Report the evidence, likely cause, and concrete suggested next steps. Include links to the failed
   checks when available, then stop.

## Confirm and merge

After all required checks pass, the review window completes without unresolved findings, and the
user explicitly permits the merge:

1. Query review feedback once more. If qualifying feedback appeared after the last poll, present it
   through the review-finding workflow and stop.
2. Query the PR again and confirm it is open, not a draft, and GitHub reports it mergeable with no
   blocked merge state or missing required review.
3. If mergeability is unknown, keep polling briefly. If it is conflicted or blocked, report the
   reason and stop without merging.
4. Merge with `gh pr merge --rebase --delete-branch`. Do not substitute a merge commit or squash
   merge. If rebase merging is unavailable, report the repository restriction and stop.
5. Verify GitHub reports the pull request as merged before changing local branches.

## Synchronize and clean up locally

1. Save the merged feature branch name, switch to local `main`, and run
   `git pull --ff-only origin main`.
2. Delete the merged local feature branch with `git branch -d`.
3. Confirm the remote feature branch was deleted; delete it explicitly if the merge command did not
   remove it, then prune remote-tracking references.
4. Confirm local `main` is clean and aligned with `origin/main`.

Report the pull request, merge commit, check results, validation commands, final branch, and branch
cleanup. Do not push additional changes, open another pull request, or modify releases.
