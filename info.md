# Base module

Shared [utilities](lib/info.md), [GPIO/motors](hardware/info.md) and
[OS tools](os/info.md). CMake output goes to `_bin`, `_test` and `_probe` without configuration suffixes.
[Platform selection](platform.cmake) sets `BIN_BASE_DIR` to the top-level
source root on desktop (this module for standalone builds), or `/` on RPI4/5 and
CM4/5. Board builds need write access to `/_bin`, `/_test` and `/_probe`.

`git-diff.sh` selects unstaged changes, then staged changes, then the latest commit
against its parent. Root `s` synchronizes parent-managed AGENTS.md/.gitignore/platform.cmake when
used as a submodule; standalone clones remain independent. Full synchronization
and gp/gi/ga behavior: [Git scripts](os/sh/info.md).
