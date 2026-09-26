# Base module

Shared [utilities](lib/info.md), [GPIO/motors](hardware/info.md) and
[OS tools](os/info.md). Standalone CMake output goes to `_bin`, `_test` and `_probe`
under this module; when included by a parent these paths use the top-level source
root, without configuration suffixes.

`git-diff.sh` selects unstaged changes, then staged changes, then the latest commit
against its parent. Root `s` synchronizes parent-managed AGENTS.md/.gitignore when
used as a submodule; standalone clones remain independent. Full synchronization
and gp/gi/ga behavior: [Git scripts](os/sh/info.md).
