---
name: po
description: Audit the current product and GitHub backlog across product strategy, user value, workflows, UX, accessibility, trust, reliability, platform fit, and adoption; deduplicate and prioritize improvement opportunities; propose well-formed GitHub issues; wait for user selection; and create only the selected issues. Use when the user invokes `$po`, asks what the product should build or improve next, requests a product or capability audit, wants backlog recommendations, or supplies an optional sentence to focus the audit on a specific topic.
---

<!-- SPDX-FileCopyrightText: 2026 Sri Rang -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Product Owner Audit

Separate discovery from issue creation. During the audit, use only read-only repository and GitHub
operations. Do not edit files, create branches, change the backlog, or treat the initial request as
permission to create issues.

## Set the audit scope

Use the user's optional sentence as the primary product lens. Keep adjacent dependencies and
cross-cutting consequences in scope, but do not turn a focused request into an unrelated full audit.
Without a focus sentence, review the product broadly.

Read all applicable `AGENTS.md` files and product evidence, including user-facing source and flows,
documentation, tests, packaging, platform integrations, recent relevant history, and release or
roadmap material. Inspect open issues, relevant closed issues, and pull requests to understand
planned, rejected, completed, or duplicated work. Run the product locally or use read-only builds
when practical and useful. Distinguish observed behavior from inference and unavailable evidence.

## Audit product capabilities

Evaluate relevant dimensions rather than forcing a finding in every category:

- product purpose, target users, and value proposition;
- core-job completeness, workflow friction, and feature coherence;
- onboarding, discoverability, UX consistency, and error recovery;
- accessibility, localization, and inclusive defaults;
- privacy, security, user trust, and control of data;
- reliability, performance, resilience, and quality signals visible to users;
- desktop or platform integration, compatibility, packaging, and distribution;
- documentation, supportability, adoption, retention, and feedback loops;
- dependencies, sequencing, and opportunities to simplify or remove low-value behavior.

Prefer improvements tied to a concrete user problem or product outcome. Include technical enablers
only when their user or delivery impact is clear. Do not manufacture backlog merely to fill a
category, restate completed work, or propose speculative features without evidence.

## Deduplicate and prioritize

Search issue titles, bodies, comments, labels, linked work, and relevant closed issues before
proposing a candidate. Exclude work already covered by an adequate open issue. When an existing
issue is related but incomplete, recommend refining or extending it instead of creating a duplicate,
and link it in the audit.

Rank candidates using user impact and reach, strategic fit, evidence and confidence, urgency,
effort, risk, dependencies, and learning value. Use:

- `P0`: urgent product safety, trust, data-loss, or release-blocking problem;
- `P1`: high-impact gap in the core user outcome or a serious adoption blocker;
- `P2`: meaningful improvement to usability, reliability, accessibility, or product completeness;
- `P3`: worthwhile optimization, experiment, or enabling improvement with limited immediate impact.

Order by priority, then expected value, confidence, and dependency sequence. Keep the proposed list
selective; omit weak candidates rather than padding it.

## Propose issue candidates

Start with a concise capability assessment: current strengths, material gaps, important assumptions,
and the focus sentence when supplied. Then present one numbered list of proposed issues. For each
candidate include:

- priority and concise issue title;
- user problem and supporting evidence;
- expected user and product outcome;
- proposed scope and explicit non-goals;
- testable acceptance criteria;
- dependencies, risks, or sequencing;
- deduplication result, including related issue links;
- prioritization rationale.

Make each candidate independently understandable as a GitHub issue. Clearly separate issue
candidates from observations that need more discovery. If no worthwhile new issue remains after
deduplication, say so explicitly.

Ask the user to select candidate numbers, ranges, `all`, or `none`, and invite revisions. Stop and
wait. Do not create any issue until the user explicitly selects the final candidates.

## Create the selection

After selection:

1. Resolve the selection against the numbered list. Clarify invalid or ambiguous choices. If the
   user selects `none`, create nothing and report that outcome.
2. Recheck the repository and GitHub backlog for changes or new duplicates. If a selected proposal
   is stale or materially needs revision, present the revision and obtain fresh approval.
3. Draft one issue per selected candidate with the approved title, problem, evidence, outcome,
   scope, non-goals, acceptance criteria, dependencies, and relevant links. Preserve product intent
   without prescribing implementation details unnecessarily.
4. Create only the selected issues in the current GitHub repository. Apply only existing labels that
   clearly fit; do not create labels, milestones, projects, or parent issues unless requested.
5. Verify every issue was created correctly and report a numbered mapping from candidate to issue
   URL. If creation partially fails, report exactly what was and was not created; do not retry in a
   way that risks duplicates without checking GitHub first.

Do not implement issues, create a development branch, close or edit other issues, or open pull
requests unless the user separately requests those actions.
