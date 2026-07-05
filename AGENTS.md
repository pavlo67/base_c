# Repository Instructions

Read info.md / task.md files and source code only within the scope explicitly specified by the user. Do not proactively explore the repository outside that scope.

Expand the scope only when there is a clear technical dependency or when the requested change cannot be implemented safely with the information available in the specified area.

When expanding the scope, inspect only the minimum additional files or directories required.



For code analysis, treat info.md in its directory as the primary source of information and if it is missing, look for it higher in the hierarchy (up to first-level subtree info.md)

Be careful: task.md files contain initial intentions but may not contain corresponding final decisions, it's ok and should not be fixed.  

Read source code only when:

- the answer cannot be obtained from info.md;
- the task requires modifying the code;
- info.md appears incomplete, inconsistent, or outdated.



Before every non-trivial action, first analyze the task and discuss likely difficulties, nuances, and specification gaps with the user. Do not start that action until the user initiates it with an explicit formal command.

Prefer existing repository patterns over new conventions.

Always look for helper functions in lib/ and hardware/ first (using corresponding `info.md`). 

Before finalizing, move all new reusable helpers from app/local files to `lib/` or `hardware/`. Treat CSV/text, bool/string conversion, math, filesystem, and formatting helpers as reusable by default.

Describe any specific behavior in `info.md` inside first-level subtree info.md (except for directory subtrees listed below). If the document is missing information, contains inaccuracies, or does not reflect the current code, inspect the subtree contents and create an info_add_<timestamp_ms>.md file with the missing information and corrections.

Don't create `info.md` in `_docs`, `_env`, `_bin`, `_sessions` and other underscored directories and its subdirectories.

Don't describe static functions in `info.md`.



Wherever possible, use error codes/messages instead of throwing exceptions.

Don't use CLI-parameters in apps — define all parameters as constants in main.cpp.

Function main() in main.cpp should be written first — after includes, constants, types, static variables and helper declarations (without bodies), of course.

Wrap error outputs as described in `_docs/common_rules.md`.

All csv-files must be stored with .xls-extension



Do not show diffs.
