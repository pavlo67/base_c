Before finalizing any implementation:

1. Search newly added/changed files for local helper functions.
2. For every helper ask:
    - Is it reusable outside this file?
    - Is it domain-independent?
    - Is it not tightly coupled to one function?
3. If yes, move it to `lib/` or `hardware/` as appropriate.
4. Do not leave reusable helpers as anonymous/local/private functions in app files.
5. CSV/text helpers, bool/string conversion helpers, math helpers, filesystem helpers, and small formatting helpers are reusable by default unless proven otherwise.