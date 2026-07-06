# Contributing Guide

This repository uses git history as part of project evaluation. Each member should keep a clean
and traceable commit trail so the team can prove individual contributions quickly.

## Branching

- `main` is the integration branch.
- Work on `feature/<module>-<name>` branches when possible.
- Merge only after the work is reviewed and the history is understandable.

## Commit style

- Keep commits small and module-focused.
- Use clear messages such as `Add ToolRegistry skeleton` or `Refine benchmark tasks`.
- Avoid mixed-topic commits.

## History checks

Useful commands for checking member contribution history:

```bash
git log --graph --oneline --decorate --all
git shortlog -sn
git log --author="Member Name"
git log --stat --author="Member Name"
```

## What to keep visible

- Frequent pushes from each member.
- Separate commits for architecture, tools, harness, tests, and docs.
- No single member should own every commit if the project is done as a group.
