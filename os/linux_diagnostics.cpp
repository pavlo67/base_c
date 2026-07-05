#include "linux_diagnostics.h"

#include <cstdio>
#include <cstdlib>
#include <unistd.h>


static void runDiagnosticCmd(FILE *log, const char *title, const char *cmd) {
    fprintf(log, "\n========== %s ==========\n", title);
    fprintf(log, "$ %s\n\n", cmd);
    fflush(log);

    char fullCmd[2048];
    snprintf(fullCmd, sizeof(fullCmd), "%s 2>&1", cmd);

    FILE *pipe = popen(fullCmd, "r");
    if (!pipe) {
        fprintf(log, "popen failed\n");
        fflush(log);
        return;
    }

    char buf[1024];
    while (fgets(buf, sizeof(buf), pipe)) {
        fputs(buf, log);
    }

    int rc = pclose(pipe);
    fprintf(log, "\n[exit code: %d]\n", rc);
    fflush(log);
}

void dumpWriteDiagnostics(const char *failedPath) {
    char logName[256];

    snprintf(
        logName,
        sizeof(logName),
        "write_diagnostics_%d.log",
        (int)getpid());

    FILE *log = fopen(logName, "w");
    if (!log) {
        printf("dumpWriteDiagnostics: cannot create diagnostics log\n");
        return;
    }

    fprintf(log, "WRITE DIAGNOSTICS\n");
    fprintf(log, "failedPath: %s\n", failedPath ? failedPath : "<null>");
    fprintf(log, "pid: %d\n", (int)getpid());
    fflush(log);

#define RUN(title, cmd) runDiagnosticCmd(log, title, cmd)

    RUN("sync", "sync");

    RUN("date", "date");
    RUN("uname", "uname -a");
    RUN("uptime", "uptime");

    RUN("disk space", "df -h");
    RUN("inode usage", "df -i");
    RUN("mounts", "mount");

    RUN("memory", "free -h");
    RUN("vmstat", "vmstat 1 5");

    char cmd[512];

    snprintf(cmd, sizeof(cmd), "cat /proc/%d/status", (int)getpid());
    RUN("process status", cmd);

    snprintf(cmd, sizeof(cmd), "cat /proc/%d/limits", (int)getpid());
    RUN("process limits", cmd);

    snprintf(cmd, sizeof(cmd), "ls -l /proc/%d/fd", (int)getpid());
    RUN("open fd list", cmd);

    snprintf(cmd, sizeof(cmd), "ls /proc/%d/fd | wc -l", (int)getpid());
    RUN("open fd count", cmd);

    RUN("vcgencmd throttled", "vcgencmd get_throttled");
    RUN("vcgencmd temp", "vcgencmd measure_temp");
    RUN("vcgencmd volts core", "vcgencmd measure_volts core");

    RUN("block devices", "lsblk");
    RUN("diskstats", "cat /proc/diskstats");

    RUN("dmesg tail", "dmesg -T | tail -300");
    RUN("dmesg storage errors",
        "dmesg -T | grep -iE 'mmc|sdhci|ext4|i/o|error|timeout|reset|readonly|corrupt|fail'");

    RUN("top snapshot", "top -b -n1 | head -80");

#undef RUN

    fclose(log);

    printf("dumpWriteDiagnostics: saved to %s\n", logName);
}


