# Git shell scripts

`gp` pulls the current branch and recursively updates submodules to recorded
commits, attaching HEADs to matching or newly created branches. After all updates,
it invokes each initialized module's root `s`, if present, parent before child.
Copied files remain working-tree changes; gp does not commit them.

`gi` invokes `s` in initialized direct submodules before any staging or commits,
then processes those modules before the current repository. It stages changes,
commits only a changed index and pushes origin HEAD even when no commit is needed.
Synchronization or Git failures stop the sequence. Commit messages use Git's editor.
`ga` uses the same direct-module order and conditional commit check, but amends and
pushes with force-with-lease; it does not synchronize files.

## Shared files

Each module-root `s` finds its superproject through Git and copies existing
AGENTS.md/.gitignore into its own root, independent of the caller's directory.
Standalone clones do nothing; missing sources preserve module files. Copying
replaces the whole file, overwriting module-specific edits. Temporary-file rename
breaks hard/symbolic links; equal independent files are not rewritten. An error
stops the caller but does not roll back earlier replacements.

These are script integrations, not Git hooks: plain Git commands bypass them.
