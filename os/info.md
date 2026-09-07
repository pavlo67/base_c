# Operating-system control

`rebootOS()` and `shutdownOS()` request a Linux reboot or power-off and return an error string when the system call fails. Builds with `MACHINA_TESTING` report success without issuing either system operation, which makes command-path integration tests safe.

`dumpWriteDiagnostics()` saves diagnostic command output to its report file. Failures to open a pipe or create the report print to stdout with ERROR: and function context; pipe errors are also recorded in the report.
