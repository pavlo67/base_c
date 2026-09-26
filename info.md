# CMake executable output

When `base` is configured by itself, regular executables go to `base/_bin`, test executables to `base/_test`, and probe executables to `base/_probe`. When included by the parent project, these directories are under the parent source root instead. Multi-configuration generators use these exact directories without a configuration suffix.

`git-diff.sh` prints the first available diff in this order: unstaged changes, staged changes, then the latest commit compared with its parent. It prints a heading identifying the selected diff.

## Parent-managed repository files

`s` copies existing `AGENTS.md` and `.gitignore` from the Git
superproject root into this module root. Paths are anchored to the script, not the
current directory. A standalone clone has no superproject and is left unchanged.
Missing source files leave module files intact. Copies replace the whole file;
module-specific edits are overwritten on the next synchronization.

Replacement uses a temporary file and rename, breaking existing hard/symbolic
links. Equal independent files are not rewritten. A failure stops the caller;
files replaced before the failure remain replaced. The script intentionally
exists in each module so it also works independently of sibling modules.

The parent's `gp` runs synchronization after updating submodules. Its `gi` runs
it before staging/committing submodules, so copied files are included in their
commits. See [Git shell scripts](os/sh/info.md).
