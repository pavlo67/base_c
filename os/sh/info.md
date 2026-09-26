# Git shell scripts

`gp` pulls the current repository, initializes/updates submodules recursively to
the recorded commits, and attaches their HEADs to suitable branches. Only after
all updates finish does it invoke each initialized submodule's root
`s`, if present, in parent-before-child traversal order.
Synchronization leaves ordinary working-tree changes; it does not commit them.

`gi` first invokes `s` in initialized direct submodules, if
present. It then stages, conditionally commits, and pushes each direct submodule,
followed by the current repository. Thus copied files enter submodule commits
before the parent records their new commit IDs. Synchronization failure aborts
before any commits/pushes; later Git failures also stop processing.

`ga` retains its existing amend/force-with-lease behavior and does not synchronize.
These scripts do not install Git hooks: plain Git commands bypass synchronization.
The sync script uses Git's superproject relationship, never arbitrary parent
folder files, so standalone clones retain their own AGENTS.md and .gitignore.
See [module synchronization](../../info.md#parent-managed-repository-files).
