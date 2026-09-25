#!/bin/sh

if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    printf '%s\n' 'git-diff.sh ERROR: current directory is not inside a Git working tree.'
    exit 1
fi

if ! git diff --quiet; then
    printf '%s\n' '=== Unstaged changes (git diff) ==='
    git diff
    exit $?
fi

if ! git diff --cached --quiet; then
    printf '%s\n' '=== Staged changes (git diff --cached) ==='
    git diff --cached
    exit $?
fi

if git rev-parse --verify HEAD^ >/dev/null 2>&1; then
    printf '%s\n' '=== Last commit (git diff HEAD^ HEAD) ==='
    git diff HEAD^ HEAD
    exit $?
fi

printf '%s\n' 'git-diff.sh: no unstaged or staged changes, and no previous commit to compare.'
exit 0
