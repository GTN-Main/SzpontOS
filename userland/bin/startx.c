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
#include <sys/ioctl.h>
#include <fcntl.h>

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
        bool exited = false;
        int status = 0;
        for (int i = 0; i < 20; i++) {
            if (waitpid(g_server_pid, &status, WNOHANG) == g_server_pid) {
                exited = true;
                break;
            }
            usleep(25000);
        }
        if (!exited) {
            kill(g_server_pid, SIGKILL);
            waitpid(g_server_pid, NULL, 0);
        }
        g_server_pid = -1;
    }
    unlink(g_sock_path);
    printf("\033[2J\033[H\033[?25h");
    fflush(stdout);
    printf(COLOR_GREEN "[startx] X11 session ended cleanly. Returning to TTY." COLOR_RESET "\n");
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
    const char *server_bin = "/usr/bin/Xorg";
    struct stat st;
    if (stat("/usr/bin/Xorg", &st) == 0) {
        server_bin = "/usr/bin/Xorg";
    } else if (stat("/bin/Xorg", &st) == 0) {
        server_bin = "/bin/Xorg";
    } else if (stat("/usr/bin/SzpontX11", &st) == 0) {
        server_bin = "/usr/bin/SzpontX11";
    } else {
        server_bin = "/bin/SzpontX11";
    }

    /* Determine default client: Prefer Display/Login Manager if available */
    const char *client_bin = "/usr/bin/szpontlogin";
    if (access("/usr/bin/szpontlogin", X_OK) != 0 && access("/bin/szpontlogin", X_OK) != 0) {
        if (access("/usr/bin/szpontdesktop", X_OK) == 0) {
            client_bin = "/usr/bin/szpontdesktop";
        } else {
            client_bin = "/bin/szpontdesktop";
        }
    } else if (access("/usr/bin/szpontlogin", X_OK) != 0) {
        client_bin = "/bin/szpontlogin";
    }
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

    /* Ensure socket directory and log directory exist */
    mkdir("/tmp", 0777);
    mkdir("/tmp/.X11-unix", 0777);
    mkdir("/var", 0755);
    mkdir("/var/log", 0755);

    /* Clean up stale socket if server not alive */
    if (!is_socket_ready(g_sock_path)) {
        unlink(g_sock_path);
    }

    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTERM, cleanup_and_exit);
    signal(SIGHUP, cleanup_and_exit);

    /* Detect DRM driver to pick hardware accelerated or software fallback config */
    const char *config_path = "/etc/X11/xorg-sw.conf";
    int drm_fd = open("/dev/dri/card0", O_RDWR);
    if (drm_fd >= 0) {
        struct {
            int version_major;
            int version_minor;
            int version_patchlevel;
            size_t name_len;
            char *name;
            size_t date_len;
            char *date;
            size_t desc_len;
            char *desc;
        } ver;
        char name_buf[64] = {0};
        memset(&ver, 0, sizeof(ver));
        ver.name = name_buf;
        ver.name_len = sizeof(name_buf) - 1;
        int hardware_accel = 0;
        if (ioctl(drm_fd, 0xc0406400, &ver) == 0) {
            if (strcmp(name_buf, "i915") == 0) {
                hardware_accel = 1;
                setenv("CROCUS_GEN8", "1", 1);
                setenv("MESA_LOADER_DRIVER_OVERRIDE", "crocus", 1);
                printf(COLOR_GREEN "[startx] Detected Intel GPU ('%s') -> 3D hardware acceleration enabled (crocus)" COLOR_RESET "\n",
                       name_buf);
            } else if (strcmp(name_buf, "virtio_gpu") == 0) {
                /* Check if host actually supports Virgl 3D hardware acceleration */
                struct {
                    uint64_t param;
                    uint64_t value;
                } gp;
                uint64_t val = 0;
                memset(&gp, 0, sizeof(gp));
                gp.param = 1; /* VIRTGPU_PARAM_3D_FEATURES */
                gp.value = (uint64_t)(uintptr_t)&val;
                if (ioctl(drm_fd, 0xc0106443, &gp) == 0 && val == 1) {
                    hardware_accel = 1;
                    unsetenv("CROCUS_GEN8");
                    setenv("MESA_LOADER_DRIVER_OVERRIDE", "virtio_gpu", 1);
                    printf(COLOR_GREEN "[startx] Detected VirtIO 3D GPU ('%s') -> 3D hardware acceleration enabled (virgl)" COLOR_RESET "\n",
                           name_buf);
                } else {
                    hardware_accel = 0;
                    unsetenv("CROCUS_GEN8");
                    unsetenv("MESA_LOADER_DRIVER_OVERRIDE");
                    printf(COLOR_CYAN "[startx] Detected 2D VirtIO GPU -> using software 3D fallback" COLOR_RESET "\n");
                }
            } else {
                hardware_accel = 0;
                unsetenv("CROCUS_GEN8");
                unsetenv("MESA_LOADER_DRIVER_OVERRIDE");
            }

            if (getenv("XORG_SW") != NULL && strcmp(getenv("XORG_SW"), "1") == 0) {
                hardware_accel = 0;
                unsetenv("MESA_LOADER_DRIVER_OVERRIDE");
                printf(COLOR_YELLOW "[startx] Forced software fallback via XORG_SW=1 (xorg-sw.conf)" COLOR_RESET "\n");
            }

            if (hardware_accel) {
                if (access("/etc/X11/xorg.conf", R_OK) == 0) {
                    config_path = "/etc/X11/xorg.conf";
                } else if (access("/etc/X11/xorg-sw.conf", R_OK) == 0) {
                    config_path = "/etc/X11/xorg-sw.conf";
                }
                printf(COLOR_GREEN "[startx] Using hardware acceleration Xorg config: %s (glamor + DRI3)" COLOR_RESET "\n", config_path);
            } else {
                if (access("/etc/X11/xorg-sw.conf", R_OK) == 0) {
                    config_path = "/etc/X11/xorg-sw.conf";
                } else if (access("/etc/X11/xorg.conf", R_OK) == 0) {
                    config_path = "/etc/X11/xorg.conf";
                }
                printf(COLOR_CYAN "[startx] Using software fallback Xorg config: %s (ShadowFB)" COLOR_RESET "\n", config_path);
            }
        }
        close(drm_fd);
    }

    /* 1. Launch X Server */
    printf(COLOR_YELLOW "[startx] Starting X11 Server: %s %s..." COLOR_RESET "\n", server_bin, display);
    g_server_pid = fork();
    if (g_server_pid < 0) {
        perror("[startx] Failed to fork X server");
        return 1;
    }

    if (g_server_pid == 0) {
        /* Child: Exec X Server */
        extern char **environ;
        if (strstr(server_bin, "Xorg")) {
            char *server_argv[] = {
                (char *)server_bin,
                (char *)display,
                (char *)"-config", (char *)config_path,
                (char *)"-nolisten", (char *)"tcp",
                NULL
            };
            execve(server_bin, server_argv, environ);
        } else {
            char *server_argv[] = { (char *)server_bin, (char *)display, NULL };
            execve(server_bin, server_argv, environ);
        }
        perror("[startx] Failed to execute X server");
        _exit(1);
    }

    /* 2. Wait for X Server to initialize socket (up to 15 seconds) */
    printf("[startx] Waiting for X server on display %s...\n", display);
    bool ready = false;
    for (int i = 0; i < 300; i++) {
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
    setenv("TERM", "xterm-256color", 0);
    setenv("COLORTERM", "truecolor", 0);
    setenv("PATH", "/bin:/usr/bin:/usr/tbin:/usr/local/bin:/sbin:/usr/sbin", 0);
    setenv("XDG_SESSION_TYPE", "x11", 0);
    setenv("XDG_CURRENT_DESKTOP", "SzpontOS", 0);
    setenv("XDG_RUNTIME_DIR", "/tmp", 0);
    setenv("USER", "root", 0);
    setenv("HOME", "/root", 0);
    setenv("SHELL", "/bin/sh", 0);
    setenv("WINDOWPATH", "1", 0);

    /* 3. Launch Graphical Client */
    printf(COLOR_YELLOW "[startx] Starting graphical session: Szpont Experience (%s)" COLOR_RESET "\n", client_bin);
    g_client_pid = fork();
    if (g_client_pid < 0) {
        perror("[startx] Failed to fork client");
        cleanup_and_exit(0);
        return 1;
    }

    if (g_client_pid == 0) {
        /* Child: Exec client with separate process group */
        setpgid(0, 0);
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        extern char **environ;
        execve(client_bin, client_args, environ);
        perror("[startx] Failed to execute client");
        _exit(1);
    }
    setpgid(g_client_pid, g_client_pid);

    /* 4. Wait for client to exit */
    int status = 0;
    waitpid(g_client_pid, &status, 0);
    g_client_pid = -1;

    printf(COLOR_CYAN "[startx] Graphical session terminated." COLOR_RESET "\n");

    /* 5. Shut down server and cleanup */
    cleanup_and_exit(0);
    return 0;
}
