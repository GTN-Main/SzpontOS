#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/sysinfo.h>

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    proc_info_t procs[64];
    int count = getprocs(procs, 64);
    if (count <= 0) {
        printf("ps: failed to retrieve process table\n");
        return 1;
    }

    printf("%-5s %-5s %-10s %-6s %s\n", "PID", "PPID", "USER", "STAT", "COMMAND");

    for (int i = 0; i < count; i++) {
        struct passwd *pwd = getpwuid(procs[i].uid);
        const char *username = pwd ? pwd->pw_name : "unknown";

        const char *stat = "R";
        if (procs[i].state == 1) stat = "Z";
        else if (procs[i].state == 2) stat = "D";

        printf("%-5d %-5d %-10s %-6s %s\n",
               procs[i].pid, procs[i].ppid, username, stat, procs[i].name);
    }

    return 0;
}
