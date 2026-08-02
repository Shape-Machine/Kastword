---
name: dev-pr
description: Validate the current Kastword feature branch, push it, create or reuse its GitHub pull request, monitor required GitHub Actions checks, merge only after checks pass and GitHub reports the PR mergeable, then synchronize local main and delete the merged feature branch locally and remotely. Use when the user invokes $dev-pr or asks Codex to open, watch, and merge the current feature branch. If checks fail, diagnose them and report suggested next steps without merging.
---

# Create, Monitor, and Merge a Pull Request

Carry the current feature branch from final validation through a merged pull request and local
cleanup. Keep the user informed during monitoring and never merge a failing, pending, conflicted, or
draft pull request.

## Prepare the branch

1. Read all applicable `AGENTS.md` files and repository instructions.
2. Record the current branch, status, upstream, and recent commits. Refuse to operate on `main` or a
   detached HEAD.
3. Require a clean working tree. Do not stash, discard, or include unrelated changes.
4. Fetch and prune `origin`. Confirm the branch contains commits not in `origin/main` and inspect the
   complete proposed diff.
5. Run every validation required by the repository. For Kastword, run `make validate` last. Stop if
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

After all required checks pass:

1. Query the PR again and confirm it is open, not a draft, and GitHub reports it mergeable with no
   blocked merge state or missing required review.
2. If mergeability is unknown, keep polling briefly. If it is conflicted or blocked, report the
   reason and stop without merging.
3. Merge using the repository's preferred strategy; use a merge commit when no preference is
   documented. Request remote branch deletion as part of the merge.
4. Verify GitHub reports the pull request as merged before changing local branches.

## Synchronize and clean up locally

1. Save the merged feature branch name, switch to local `main`, and run
   `git pull --ff-only origin main`.
2. Delete the merged local feature branch with `git branch -d`.
3. Confirm the remote feature branch was deleted; delete it explicitly if the merge command did not
   remove it, then prune remote-tracking references.
4. Confirm local `main` is clean and aligned with `origin/main`.

Report the pull request, merge commit, check results, validation commands, final branch, and branch
cleanup. Do not push additional changes, open another pull request, or modify releases.
