# Shared utilities

General-purpose utilities independent of image processing. Main entry headers:
[filelib.h](filelib.h), [strlib.h](strlib.h), [csvlib.h](csvlib.h),
[mathlib.h](mathlib.h), [timelib.h](timelib.h), [execlib.h](execlib.h) and
[config/config.h](config/config.h). Network ownership is described in
[server](server/info.md).

## Files and process execution

Path splitting uses Unix `/` conventions. `listOfFiles()` returns sorted names,
not full paths; an empty result can mean either no matches or an open failure.
`readFileByLines()` appends without clearing its vector; `readFile()` clears its
string and appends a newline per line. `writeFile()` can open/truncate a file even
when content is null. `newPath()` creates parents and returns a trailing slash,
or an empty string on failure. `newFile()` returns a caller-owned `FILE*`.

Directory checks/creation, file-size comparison, removal and safe filename parts
are shared here. `moveFileReplacing()` tries rename, then overwrite-copy and source
removal; it is not an atomic cross-filesystem move. `cleanupDirectory()` removes
contents recursively. `createUniqueDirectory()` atomically reserves a fresh run
directory using a timestamp/suffix (up to 100 attempts); prefix must be nonempty
without separators, and failure preserves the caller's output path.

`exec()` reads stdout through popen, appending to a supplied string or printing
trimmed nonempty lines. It does not separately capture stderr or validate the
command's exit status; true is not proof that the command succeeded.

## Strings, configuration and math

`split()` currently advances one character after a match, so only single-character
delimiters behave correctly. `parseFiniteFloat()` accepts complete signed decimal/
scientific input, rejects nonfinite/out-of-range values and preserves output on
failure. `logger()` duplicates printf-style output to an optional file.
CSV escaping quotes special characters; `boolCsv()` produces 1/0.

`Config` contains a YAML document. Load errors are reported rather than propagated;
`get()` returns a node handle by value to avoid a dangling map-lookup reference.
`Vec2D` helpers include normalization toward the right half-plane and an acute
angle to horizontal; these conventions matter to image orientation consumers.

## Time

`Clock` is steady/monotonic, shared by schedulers. `monotonicNowMs()` uses its
unspecified epoch; `internal32Ms()` measures time since library startup and wraps
at 32 bits. `now()`/`nowMs()` instead use CLOCK_REALTIME; formatted time is local.
Do not mix clock domains for scheduling.

`Timing` accumulates nanosecond count/average/min/max measurements. Reaching
`afterCnt` resets statistics; `resetEachCnt` is stored but not implemented.
