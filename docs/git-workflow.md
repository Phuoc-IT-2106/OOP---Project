# Git Workflow

This project is maintained with git so reviewers can inspect commit and push history by member.

## Recommended flow

1. Create a working branch for the module.
2. Make one logical change per commit.
3. Push frequently so remote history stays visible.
4. Keep commit messages descriptive and module-based.
5. Before submission, review per-member history with `git shortlog -sn` and `git log --author`.

## Suggested branch names

- `feature/client-<name>`
- `feature/tools-<name>`
- `feature/harness-<name>`
- `feature/docs-<name>`

## Suggested commit message patterns

- `Add ToolRegistry skeleton`
- `Implement loop detector`
- `Document benchmark tasks`
- `Refine harness trajectory format`

## Review checklist

- Each member has visible commits.
- Commits map to a specific module.
- The history is easy to explain during presentation.
- No large undocumented rewrite is buried in one commit.
