# Operating-system tools

`rebootOS()` and `shutdownOS()` request Linux power operations and return errors
through strings. MACHINA_TESTING suppresses the operations for integration tests.
`dumpWriteDiagnostics()` records command output/errors in a diagnostic file.

## Linux health monitor

`health_monitor` in `health/` runs without CLI parameters and samples once per
second. It shows CPU/load, memory/swap and uptime; Raspberry Pi builds additionally
use vcgencmd for temperature, throttling and voltage. Missing desktop sensors are
informational. Problem thresholds include nonzero throttling, temperature >=80 C,
available memory <10%, and one-minute load above logical CPU count.

Kernel warnings present at startup form the baseline; only new ones are problems.
Unavailable dmesg disables that check. Space pauses both sampling and refresh,
then resumes immediately; q, Ctrl+C and SIGTERM exit with terminal cleanup.
`SCREEN_HEIGHT` controls the display; launcher and monitor logic are separate.

[Git shell scripts](sh/info.md) describe pull, commit/push and submodule-file sync.
