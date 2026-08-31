/*
 * SzpontOS — startx (X11 Graphical Session Launcher)
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * Spawns SzpontX11 native X server, sets up DISPLAY=:0, launches default or
 * specified X11 client/WM session, and cleans up on session exit.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>

#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"

static pid_t g_server_pid = -1;
static pid_t g_client_pid = -1;
static char g_sock_path[128] = "/tmp/.X11-unix/X0";

static void cleanup_and_exit(int sig) {
    (void)sig;
    if (g_client_pid > 0) {
        kill(g_client_pid, SIGTERM);
        waitpid(g_client_pid, NULL, 0);
        g_client_pid = -1;
    }
    if (g_server_pid > 0) {
        printf(COLOR_CYAN "[startx] Terminating SzpontX11 server (PID %d)..." COLOR_RESET "\n", g_server_pid);
        kill(g_server_pid, SIGTERM);
        usleep(50000);
        kill(g_server_pid, SIGKILL);
        waitpid(g_server_pid, NULL, 0);
        g_server_pid = -1;
    }
    unlink(g_sock_path);
    printf(COLOR_GREEN "[startx] X11 session ended cleanly." COLOR_RESET "\n");
    exit(0);
}

static bool is_socket_ready(const char *path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return false;
    }
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    int res = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    close(fd);
    return (res == 0);
}

static void print_banner(void) {
    printf(COLOR_CYAN "========================================================\n" COLOR_RESET);
    printf(COLOR_BOLD "  SzpontOS startx — Szpont Experience Session Launcher\n" COLOR_RESET);
    printf("  (C) Copyright by Szpont Industries. All rights reserved.\n");
    printf(COLOR_CYAN "========================================================\n" COLOR_RESET);
}

int main(int argc, char *argv[]) {
    print_banner();

    const char *display = ":0";
    const char *server_bin = "/bin/SzpontX11";

    /* Determine default client */
    const char *client_bin = "/bin/szpontdesktop";
    char *client_args[16];
    int client_argc = 0;

    if (argc > 1) {
        client_bin = argv[1];
        client_args[client_argc++] = argv[1];
        for (int i = 2; i < argc && client_argc < 14; i++) {
            client_args[client_argc++] = argv[i];
        }
    } else {
        client_args[client_argc++] = (char *)client_bin;
        client_args[client_argc++] = (char *)display;
    }
    client_args[client_argc] = NULL;

    /* Ensure socket directory exists */
    mkdir("/tmp", 0777);
    mkdir("/tmp/.X11-unix", 0777);

    /* Clean up stale socket if server not alive */
    if (!is_socket_ready(g_sock_path)) {
        unlink(g_sock_path);
    }

    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);
    signal(SIGHUP, cleanup_and_exit);

    /* 1. Launch X Server */
    printf(COLOR_YELLOW "[startx] Starting X11 Server: %s %s..." COLOR_RESET "\n", server_bin, display);
    g_server_pid = fork();
    if (g_server_pid < 0) {
        perror("[startx] Failed to fork X server");
        return 1;
    }

    if (g_server_pid == 0) {
        /* Child: Exec X Server */
        char *server_argv[] = { (char *)server_bin, (char *)display, NULL };
        execve(server_bin, server_argv, NULL);
        /* Fallback if /bin/SzpontX11 fails */
        char *fallback_argv[] = { (char *)"/bin/Xorg", (char *)display, NULL };
        execve("/bin/Xorg", fallback_argv, NULL);
        perror("[startx] Failed to execute X server");
        _exit(1);
    }

    /* 2. Wait for X Server to initialize socket */
    printf("[startx] Waiting for X server on display %s...\n", display);
    bool ready = false;
    for (int i = 0; i < 40; i++) {
        usleep(50000); /* 50 ms */
        if (is_socket_ready(g_sock_path)) {
            ready = true;
            break;
        }
    }

    if (!ready) {
        printf(COLOR_RED "[startx] Error: X server failed to initialize within timeout!" COLOR_RESET "\n");
        cleanup_and_exit(0);
        return 1;
    }

    printf(COLOR_GREEN "[startx] X server ready! Setting DISPLAY=%s" COLOR_RESET "\n", display);
    setenv("DISPLAY", display, 1);

    /* 3. Launch Graphical Client */
    printf(COLOR_YELLOW "[startx] Starting graphical session: Szpont Experience (%s)" COLOR_RESET "\n", client_bin);
    g_client_pid = fork();
    if (g_client_pid < 0) {
        perror("[startx] Failed to fork client");
        cleanup_and_exit(0);
        return 1;
    }

    if (g_client_pid == 0) {
        /* Child: Exec client */
        execve(client_bin, client_args, NULL);
        perror("[startx] Failed to execute client");
        _exit(1);
    }

    /* 4. Wait for client to exit */
    int status = 0;
    waitpid(g_client_pid, &status, 0);
    g_client_pid = -1;

    printf(COLOR_CYAN "[startx] Graphical session terminated." COLOR_RESET "\n");

    /* 5. Shut down server and cleanup */
    cleanup_and_exit(0);
    return 0;
}
