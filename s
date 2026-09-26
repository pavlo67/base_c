#!/bin/sh

# This file intentionally lives in each submodule so standalone clones are independent.
set -eu

context='[s]'
temporary_file=''
trap 'if [ -n "$temporary_file" ]; then rm -f -- "$temporary_file"; fi' EXIT
trap 'exit 1' HUP INT TERM

script_directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P) || exit 1
parent_directory=$(git -C "$script_directory" rev-parse --show-superproject-working-tree) || exit 1
if [ -z "$parent_directory" ]; then
    exit 0
fi

for name in AGENTS.md .gitignore; do
    source_file="$parent_directory/$name"
    target_file="$script_directory/$name"
    if [ ! -f "$source_file" ]; then
        continue
    fi
    if [ -d "$target_file" ]; then
        printf '%s ERROR: destination is a directory: %s\n' "$context" "$target_file"
        exit 1
    fi
    # Equal independent files need no rewrite; replace links even when content matches.
    if [ -f "$target_file" ] && [ ! -L "$target_file" ] &&
       [ ! "$source_file" -ef "$target_file" ] && cmp -s -- "$source_file" "$target_file"; then
        continue
    fi
    temporary_file=$(mktemp "$script_directory/.sync_parent_files.XXXXXX") || {
        printf '%s ERROR: cannot create temporary file in %s\n' "$context" "$script_directory"
        exit 1
    }
    if ! cp -p -- "$source_file" "$temporary_file" || ! mv -f -- "$temporary_file" "$target_file"; then
        printf '%s ERROR: cannot copy %s to %s\n' "$context" "$source_file" "$target_file"
        exit 1
    fi
    temporary_file=''
    printf '%s Updated %s\n' "$context" "$target_file"
done
