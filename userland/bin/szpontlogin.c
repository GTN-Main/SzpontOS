/*
 * SzpontOS — Graphical Login & Display Manager (szpontlogin)
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * Dedicated X11 Display Manager running prior to the user desktop session.
 * Features:
 *   - Cyber-glassmorphic graphical interface with glowing electric cyan accents
 *   - Pre-selected default user "szpont" with single-click/Enter login
 *   - User account selection (szpont / root / custom entry)
 *   - PAM/POSIX credential authentication against /etc/passwd and /etc/shadow
 *   - Privilege dropping via initgroups(), setgid(), setuid()
 *   - Clean environment configuration (HOME, USER, LOGNAME, PATH, DISPLAY, XDG_*)
 *   - Spawns /bin/szpontdesktop under authenticated user credentials
 *   - Session supervisor: re-displays login card upon user logout
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <shadow.h>
#include <crypt.h>
#include <grp.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <fcntl.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#define CARD_W 560
#define CARD_H 440

typedef enum {
    FOCUS_PASSWORD,
    FOCUS_USERNAME,
    FOCUS_LOGIN_BTN
} focus_field_t;

static unsigned long make_rgb(Display *dpy, int screen,
                              unsigned short r, unsigned short g, unsigned short b) {
    XColor col;
    Colormap cmap = DefaultColormap(dpy, screen);
    col.red = r;
    col.green = g;
    col.blue = b;
    col.flags = DoRed | DoGreen | DoBlue;
    if (XAllocColor(dpy, cmap, &col)) {
        return col.pixel;
    }
    return WhitePixel(dpy, screen);
}

static void draw_rounded_box(Display *dpy, Window win, GC gc,
                             int x, int y, int w, int h, unsigned long fill_col, unsigned long border_col) {
    XSetForeground(dpy, gc, fill_col);
    XFillRectangle(dpy, win, gc, x, y, (unsigned int)w, (unsigned int)h);
    XSetForeground(dpy, gc, border_col);
    XDrawRectangle(dpy, win, gc, x, y, (unsigned int)(w - 1), (unsigned int)(h - 1));
}

static void render_login_card(Display *dpy, Window win, GC gc,
                             const char *username, const char *password,
                             focus_field_t focus, const char *status_msg, bool is_error,
                             unsigned long bg_col, unsigned long card_col,
                             unsigned long border_col, unsigned long cyan_col,
                             unsigned long indigo_col, unsigned long text_col,
                             unsigned long text_dim, unsigned long input_bg,
                             unsigned long green_col, unsigned long red_col,
                             unsigned long btn_bg, unsigned long btn_text) {
    (void)bg_col;

    /* 1. Clear Card Background */
    XSetForeground(dpy, gc, card_col);
    XFillRectangle(dpy, win, gc, 0, 0, CARD_W, CARD_H);

    /* 2. Outer Card Border (Glowing Electric Cyan) */
    XSetForeground(dpy, gc, cyan_col);
    XDrawRectangle(dpy, win, gc, 0, 0, CARD_W - 1, CARD_H - 1);
    XDrawRectangle(dpy, win, gc, 1, 1, CARD_W - 3, CARD_H - 3);

    /* 3. Header Banner */
    XSetForeground(dpy, gc, cyan_col);
    XDrawString(dpy, win, gc, 32, 44, "SZPONTOS DISPLAY MANAGER", 24);

    XSetForeground(dpy, gc, text_dim);
    XDrawString(dpy, win, gc, 32, 64, "Szpont Experience — Secure Session Login", 39);

    /* Header separator line */
    XSetForeground(dpy, gc, indigo_col);
    XDrawLine(dpy, win, gc, 32, 78, CARD_W - 32, 78);

    /* 4. Quick User Selectors */
    XSetForeground(dpy, gc, text_col);
    XDrawString(dpy, win, gc, 32, 106, "Select User Account:", 20);

    bool is_szpont = (strcmp(username, "szpont") == 0);
    bool is_root   = (strcmp(username, "root") == 0);

    /* 'szpont' badge */
    draw_rounded_box(dpy, win, gc, 32, 118, 140, 36,
                     is_szpont ? indigo_col : input_bg,
                     is_szpont ? cyan_col : border_col);
    XSetForeground(dpy, gc, is_szpont ? cyan_col : text_dim);
    XDrawString(dpy, win, gc, 44, 140, "[*] szpont (Default)", 20);

    /* 'root' badge */
    draw_rounded_box(dpy, win, gc, 185, 118, 120, 36,
                     is_root ? indigo_col : input_bg,
                     is_root ? cyan_col : border_col);
    XSetForeground(dpy, gc, is_root ? cyan_col : text_dim);
    XDrawString(dpy, win, gc, 200, 140, "[#] root", 8);

    /* 5. Username Input Box */
    XSetForeground(dpy, gc, text_col);
    XDrawString(dpy, win, gc, 32, 185, "Username:", 9);

    draw_rounded_box(dpy, win, gc, 32, 195, CARD_W - 64, 38,
                     input_bg, (focus == FOCUS_USERNAME) ? cyan_col : border_col);
    XSetForeground(dpy, gc, (focus == FOCUS_USERNAME) ? cyan_col : text_col);
    char u_display[128];
    snprintf(u_display, sizeof(u_display), "%s%s", username, (focus == FOCUS_USERNAME) ? "|" : "");
    XDrawString(dpy, win, gc, 46, 219, u_display, (int)strlen(u_display));

    /* 6. Password Input Box */
    XSetForeground(dpy, gc, text_col);
    XDrawString(dpy, win, gc, 32, 260, "Password:", 9);

    draw_rounded_box(dpy, win, gc, 32, 270, CARD_W - 64, 38,
                     input_bg, (focus == FOCUS_PASSWORD) ? cyan_col : border_col);

    char p_display[128];
    size_t pass_len = strlen(password);
    if (pass_len > sizeof(p_display) - 2) pass_len = sizeof(p_display) - 2;
    for (size_t i = 0; i < pass_len; i++) p_display[i] = '*';
    if (focus == FOCUS_PASSWORD) {
        p_display[pass_len] = '|';
        p_display[pass_len + 1] = '\0';
    } else {
        p_display[pass_len] = '\0';
    }

    if (pass_len == 0 && focus != FOCUS_PASSWORD) {
        XSetForeground(dpy, gc, text_dim);
        char ph[64];
        snprintf(ph, sizeof(ph), "(Enter password for %s)", username);
        XDrawString(dpy, win, gc, 46, 294, ph, (int)strlen(ph));
    } else {
        XSetForeground(dpy, gc, cyan_col);
        XDrawString(dpy, win, gc, 46, 294, p_display, (int)strlen(p_display));
    }

    /* 7. Action Button: [ Log In ] */
    draw_rounded_box(dpy, win, gc, 32, 335, CARD_W - 64, 44,
                     (focus == FOCUS_LOGIN_BTN) ? cyan_col : btn_bg,
                     (focus == FOCUS_LOGIN_BTN) ? text_col : cyan_col);
    XSetForeground(dpy, gc, btn_text);
    XDrawString(dpy, win, gc, (CARD_W / 2) - 30, 362, "LOG IN", 6);

    /* 8. Status Label */
    XSetForeground(dpy, gc, is_error ? red_col : (status_msg ? green_col : text_dim));
    const char *msg = status_msg ? status_msg : "Ready. Enter password to log in.";
    XDrawString(dpy, win, gc, 32, 405, msg, (int)strlen(msg));

    /* 9. Small Footer Power Controls */
    XSetForeground(dpy, gc, text_dim);
    XDrawString(dpy, win, gc, 32, 428, "[F1] Reboot", 11);
    XDrawString(dpy, win, gc, 130, 428, "[F2] Power Off", 14);
    XDrawString(dpy, win, gc, CARD_W - 200, 428, "Szpont Industries (C) 2026", 26);
}

static bool authenticate_user(const char *username, const char *password, struct passwd **out_pw) {
    if (!username || !*username) return false;

    struct passwd *pw = getpwnam(username);
    if (!pw) return false;

    if (out_pw) *out_pw = pw;

    /* Retrieve password from /etc/shadow or /etc/passwd */
    const char *stored = NULL;
    struct spwd *sp = getspnam(username);
    if (sp && sp->sp_pwdp) {
        stored = sp->sp_pwdp;
    } else if (pw->pw_passwd && strcmp(pw->pw_passwd, "x") != 0) {
        stored = pw->pw_passwd;
    }

    if (!stored) {
        return false;
    }

    /* Locked account: starts with '*' or '!' */
    if (stored[0] == '*' || stored[0] == '!') {
        return false;
    }

    const char *entered = password ? password : "";

    /* If stored password is empty, user must enter empty password */
    if (stored[0] == '\0') {
        return (entered[0] == '\0');
    }

    /* Stored password is non-empty: empty entered password must fail */
    if (entered[0] == '\0') {
        return false;
    }

    /* Verify with crypt() */
    char *hash = crypt(entered, stored);
    if (hash && strcmp(hash, stored) == 0) {
        return true;
    }

    return false;
}

static const unsigned char cursor_bits[] = {
    0x01, 0x00, 0x03, 0x00, 0x07, 0x00, 0x0f, 0x00,
    0x1f, 0x00, 0x3f, 0x00, 0x7f, 0x00, 0xff, 0x00,
    0x7f, 0x00, 0x1f, 0x00, 0x3b, 0x00, 0x71, 0x00,
    0xe0, 0x00, 0xc0, 0x01, 0x80, 0x01, 0x00, 0x00
};
static const unsigned char cursor_mask[] = {
    0x03, 0x00, 0x07, 0x00, 0x0f, 0x00, 0x1f, 0x00,
    0x3f, 0x00, 0x7f, 0x00, 0xff, 0x00, 0xff, 0x01,
    0xff, 0x01, 0xff, 0x00, 0x7f, 0x00, 0xfb, 0x00,
    0xf1, 0x01, 0xe0, 0x03, 0xc0, 0x03, 0x80, 0x01
};

static Cursor create_default_cursor(Display *dpy, Window root) {
    Pixmap src = XCreateBitmapFromData(dpy, root, (const char *)cursor_bits, 16, 16);
    Pixmap msk = XCreateBitmapFromData(dpy, root, (const char *)cursor_mask, 16, 16);
    XColor fg, bg;
    fg.red = 0xffff; fg.green = 0xffff; fg.blue = 0xffff; fg.flags = DoRed|DoGreen|DoBlue;
    bg.red = 0x0000; bg.green = 0x0000; bg.blue = 0x0000; bg.flags = DoRed|DoGreen|DoBlue;
    Cursor c = XCreatePixmapCursor(dpy, src, msk, &fg, &bg, 0, 0);
    XFreePixmap(dpy, src);
    XFreePixmap(dpy, msk);
    return c;
}

int main(int argc, char *argv[]) {
    const char *disp_name = (argc > 1) ? argv[1] : getenv("DISPLAY");
    if (!disp_name || !*disp_name) disp_name = ":0";

    printf("[szpontlogin] Starting SzpontOS Display Manager on '%s'...\n", disp_name);

    Display *dpy = XOpenDisplay(disp_name);
    if (!dpy) {
        fprintf(stderr, "[szpontlogin] Cannot open display '%s'\n", disp_name);
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);
    fcntl(ConnectionNumber(dpy), F_SETFD, FD_CLOEXEC);

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);
    int screen_w = DisplayWidth(dpy, screen);
    int screen_h = DisplayHeight(dpy, screen);

    /* Palette */
    unsigned long bg_col     = make_rgb(dpy, screen, 0x0b0b, 0x0f0f, 0x1919); /* Void black #0b0f19 */
    unsigned long card_col   = make_rgb(dpy, screen, 0x1515, 0x1a1a, 0x2828); /* Card #151a28 */
    unsigned long border_col = make_rgb(dpy, screen, 0x3333, 0x4141, 0x5555); /* Border #334155 */
    unsigned long cyan_col   = make_rgb(dpy, screen, 0x0000, 0xf0f0, 0xffff); /* Electric Cyan #00f0ff */
    unsigned long indigo_col = make_rgb(dpy, screen, 0x4343, 0x3838, 0xcaca); /* Deep indigo */
    unsigned long text_col   = make_rgb(dpy, screen, 0xf8f8, 0xfafa, 0xfcfc); /* White text */
    unsigned long text_dim   = make_rgb(dpy, screen, 0x9494, 0xa3a3, 0xb8b8); /* Slate text */
    unsigned long input_bg   = make_rgb(dpy, screen, 0x0e0e, 0x1212, 0x1d1d); /* Input background */
    unsigned long green_col  = make_rgb(dpy, screen, 0x1010, 0xb9b9, 0x8181); /* Green */
    unsigned long red_col    = make_rgb(dpy, screen, 0xefef, 0x4444, 0x4444); /* Red */
    unsigned long btn_bg     = make_rgb(dpy, screen, 0x0000, 0xd0d0, 0xe0e0); /* Button Cyan */
    unsigned long btn_text   = make_rgb(dpy, screen, 0x0a0a, 0x0e0e, 0x1717); /* Button text */

    /* Paint Root Window with deep dark canvas */
    XSetWindowBackground(dpy, root, bg_col);
    XClearWindow(dpy, root);

    /* Create Centered Login Card Window */
    int win_x = (screen_w - CARD_W) / 2;
    int win_y = (screen_h - CARD_H) / 2;
    if (win_x < 0) win_x = 0;
    if (win_y < 0) win_y = 0;

    Window win = XCreateSimpleWindow(dpy, root, win_x, win_y,
                                     CARD_W, CARD_H, 2, cyan_col, card_col);

    XSetWindowAttributes win_attr;
    win_attr.override_redirect = True;
    XChangeWindowAttributes(dpy, win, CWOverrideRedirect, &win_attr);

    Cursor cur = create_default_cursor(dpy, root);
    if (cur != None) {
        XDefineCursor(dpy, root, cur);
        XDefineCursor(dpy, win, cur);
    }

    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask);
    XMapWindow(dpy, win);
    XRaiseWindow(dpy, win);
    XSetInputFocus(dpy, win, RevertToParent, CurrentTime);

    GC gc = XCreateGC(dpy, win, 0, NULL);

    char username_buf[64] = "szpont";
    char password_buf[64] = "";
    focus_field_t focus = FOCUS_PASSWORD;
    char status_msg[128] = "Welcome to SzpontOS. Enter password to log in.";
    bool is_error = false;

    printf("[szpontlogin] Display Manager window ready on display %s.\n", disp_name);
    fflush(stdout);

    render_login_card(dpy, win, gc, username_buf, password_buf, focus,
                      status_msg, is_error, bg_col, card_col, border_col,
                      cyan_col, indigo_col, text_col, text_dim, input_bg,
                      green_col, red_col, btn_bg, btn_text);
    XFlush(dpy);

    XEvent ev;
    while (1) {
        XNextEvent(dpy, &ev);

        switch (ev.type) {
        case Expose:
            if (ev.xexpose.count == 0) {
                render_login_card(dpy, win, gc, username_buf, password_buf, focus,
                                  status_msg, is_error, bg_col, card_col, border_col,
                                  cyan_col, indigo_col, text_col, text_dim, input_bg,
                                  green_col, red_col, btn_bg, btn_text);
                XFlush(dpy);
            }
            break;

        case ButtonPress: {
            int mx = ev.xbutton.x;
            int my = ev.xbutton.y;

            /* User selector: 'szpont' badge (x: 32..172, y: 118..154) */
            if (mx >= 32 && mx <= 172 && my >= 118 && my <= 154) {
                strncpy(username_buf, "szpont", sizeof(username_buf) - 1);
                password_buf[0] = '\0';
                focus = FOCUS_PASSWORD;
                is_error = false;
                snprintf(status_msg, sizeof(status_msg), "Selected user 'szpont'. Enter password.");
            }
            /* User selector: 'root' badge (x: 185..305, y: 118..154) */
            else if (mx >= 185 && mx <= 305 && my >= 118 && my <= 154) {
                strncpy(username_buf, "root", sizeof(username_buf) - 1);
                password_buf[0] = '\0';
                focus = FOCUS_PASSWORD;
                is_error = false;
                snprintf(status_msg, sizeof(status_msg), "Selected user 'root'. Enter password.");
            }
            /* Username input box (x: 32..528, y: 195..233) */
            else if (mx >= 32 && mx <= CARD_W - 32 && my >= 195 && my <= 233) {
                focus = FOCUS_USERNAME;
            }
            /* Password input box (x: 32..528, y: 270..308) */
            else if (mx >= 32 && mx <= CARD_W - 32 && my >= 270 && my <= 308) {
                focus = FOCUS_PASSWORD;
            }
            /* Log In button (x: 32..528, y: 335..379) */
            else if (mx >= 32 && mx <= CARD_W - 32 && my >= 335 && my <= 379) {
                goto do_authenticate;
            }
            /* Reboot shortcut (x: 32..100, y: 415..435) */
            else if (mx >= 32 && mx <= 110 && my >= 415 && my <= 435) {
                sync();
                reboot(RB_AUTOBOOT);
            }
            /* Power off shortcut (x: 130..220, y: 415..435) */
            else if (mx >= 130 && mx <= 230 && my >= 415 && my <= 435) {
                sync();
                reboot(RB_POWER_OFF);
            }

            render_login_card(dpy, win, gc, username_buf, password_buf, focus,
                              status_msg, is_error, bg_col, card_col, border_col,
                              cyan_col, indigo_col, text_col, text_dim, input_bg,
                              green_col, red_col, btn_bg, btn_text);
            XFlush(dpy);
            break;
        }

        case KeyPress: {
            char kbuf[32];
            KeySym ksym = NoSymbol;
            int len = XLookupString(&ev.xkey, kbuf, sizeof(kbuf) - 1, &ksym, NULL);

            if (ksym == XK_F1) {
                sync();
                reboot(RB_AUTOBOOT);
                break;
            } else if (ksym == XK_F2) {
                sync();
                reboot(RB_POWER_OFF);
                break;
            } else if (ksym == XK_Tab) {
                /* Cycle focus */
                if (focus == FOCUS_USERNAME) focus = FOCUS_PASSWORD;
                else if (focus == FOCUS_PASSWORD) focus = FOCUS_LOGIN_BTN;
                else focus = FOCUS_USERNAME;
            } else if (ksym == XK_Up || ksym == XK_Down) {
                /* Toggle between szpont and root */
                if (strcmp(username_buf, "szpont") == 0) {
                    strncpy(username_buf, "root", sizeof(username_buf) - 1);
                    snprintf(status_msg, sizeof(status_msg), "Selected user 'root'. Enter password.");
                } else {
                    strncpy(username_buf, "szpont", sizeof(username_buf) - 1);
                    snprintf(status_msg, sizeof(status_msg), "Selected user 'szpont'. Enter password.");
                }
                password_buf[0] = '\0';
                focus = FOCUS_PASSWORD;
                is_error = false;
            } else if (ksym == XK_Return || ksym == XK_KP_Enter) {
                if (focus == FOCUS_USERNAME) {
                    focus = FOCUS_PASSWORD;
                    render_login_card(dpy, win, gc, username_buf, password_buf, focus,
                                      status_msg, is_error, bg_col, card_col, border_col,
                                      cyan_col, indigo_col, text_col, text_dim, input_bg,
                                      green_col, red_col, btn_bg, btn_text);
                    XFlush(dpy);
                    break;
                }
                goto do_authenticate;
            } else if (ksym == XK_BackSpace) {
                if (focus == FOCUS_USERNAME) {
                    size_t ulen = strlen(username_buf);
                    if (ulen > 0) username_buf[ulen - 1] = '\0';
                } else if (focus == FOCUS_PASSWORD) {
                    size_t plen = strlen(password_buf);
                    if (plen > 0) password_buf[plen - 1] = '\0';
                }
            } else if (ksym == XK_Escape) {
                password_buf[0] = '\0';
            } else if (len > 0 && (unsigned char)kbuf[0] >= 32) {
                /* Regular character typing */
                if (focus == FOCUS_USERNAME) {
                    size_t ulen = strlen(username_buf);
                    if (ulen < sizeof(username_buf) - 2) {
                        username_buf[ulen] = kbuf[0];
                        username_buf[ulen + 1] = '\0';
                    }
                } else if (focus == FOCUS_PASSWORD) {
                    size_t plen = strlen(password_buf);
                    if (plen < sizeof(password_buf) - 2) {
                        password_buf[plen] = kbuf[0];
                        password_buf[plen + 1] = '\0';
                    }
                }
            }

            render_login_card(dpy, win, gc, username_buf, password_buf, focus,
                              status_msg, is_error, bg_col, card_col, border_col,
                              cyan_col, indigo_col, text_col, text_dim, input_bg,
                              green_col, red_col, btn_bg, btn_text);
            XFlush(dpy);
            break;
        }
        }

        continue;

do_authenticate: ;
        struct passwd *pw = NULL;
        bool ok = authenticate_user(username_buf, password_buf, &pw);
        if (!ok || !pw) {
            printf("[szpontlogin] Authentication failed for user '%s'\n", username_buf);
            fflush(stdout);
            is_error = true;
            snprintf(status_msg, sizeof(status_msg), "Authentication failed for '%s'! Invalid password.", username_buf);
            password_buf[0] = '\0';
            focus = FOCUS_PASSWORD;
            render_login_card(dpy, win, gc, username_buf, password_buf, focus,
                              status_msg, is_error, bg_col, card_col, border_col,
                              cyan_col, indigo_col, text_col, text_dim, input_bg,
                              green_col, red_col, btn_bg, btn_text);
            XFlush(dpy);
            continue;
        }

        /* Authentication successful! */
        is_error = false;
        snprintf(status_msg, sizeof(status_msg), "Authentication successful! Launching desktop...");
        render_login_card(dpy, win, gc, username_buf, password_buf, focus,
                          status_msg, is_error, bg_col, card_col, border_col,
                          cyan_col, indigo_col, text_col, text_dim, input_bg,
                          green_col, red_col, btn_bg, btn_text);
        XFlush(dpy);

        printf("[szpontlogin] User '%s' (UID %u, GID %u) authenticated. Spawning session...\n",
               pw->pw_name, (unsigned int)pw->pw_uid, (unsigned int)pw->pw_gid);

        /* Hide login card while desktop session is active */
        XUnmapWindow(dpy, win);
        XFlush(dpy);

        pid_t pid = fork();
        if (pid < 0) {
            perror("[szpontlogin] fork failed");
            XMapWindow(dpy, win);
            snprintf(status_msg, sizeof(status_msg), "Error: Failed to spawn session!");
            is_error = true;
            continue;
        }

        if (pid == 0) {
            /* Child process: Close X11 connection socket without sending X_CloseDown protocol packet */
            close(ConnectionNumber(dpy));

            /* Detach and establish user session process group */
            setsid();

            /* 1. Supplementary Groups & Primary Credentials */
            initgroups(pw->pw_name, pw->pw_gid);
            if (setgid(pw->pw_gid) != 0) {
                perror("[szpontlogin] setgid failed");
            }
            if (setuid(pw->pw_uid) != 0) {
                perror("[szpontlogin] setuid failed");
            }

            /* 2. Switch Working Directory */
            if (pw->pw_dir && *pw->pw_dir) {
                chdir(pw->pw_dir);
            }

            /* 3. Export clean user environment */
            setenv("USER", pw->pw_name, 1);
            setenv("LOGNAME", pw->pw_name, 1);
            setenv("HOME", pw->pw_dir ? pw->pw_dir : "/", 1);
            setenv("SHELL", pw->pw_shell ? pw->pw_shell : "/bin/sh", 1);
            setenv("DISPLAY", disp_name, 1);
            setenv("PATH", "/bin:/usr/bin:/usr/tbin:/usr/local/bin:/sbin:/usr/sbin", 1);
            setenv("TERM", "xterm-256color", 0);
            setenv("COLORTERM", "truecolor", 0);
            setenv("XDG_CURRENT_DESKTOP", "SzpontOS", 1);
            setenv("XDG_SESSION_TYPE", "x11", 1);
            setenv("XDG_RUNTIME_DIR", "/tmp", 0);
            setenv("ENV", "/etc/shrc", 0);

            /* 4. Exec /bin/szpontdesktop */
            char *session_argv[] = {(char *)"/bin/szpontdesktop", NULL};
            extern char **environ;
            execve("/bin/szpontdesktop", session_argv, environ);

            perror("[szpontlogin] Failed to execute /bin/szpontdesktop");
            _exit(1);
        }

        /* Parent process (running as root supervisor): wait for session termination */
        int status = 0;
        waitpid(pid, &status, 0);
        printf("[szpontlogin] Session for user '%s' ended (status: %d).\n", pw->pw_name, status);

        /* Terminate any remaining processes in the user's session process group */
        kill(-pid, SIGTERM);
        usleep(50000);
        kill(-pid, SIGKILL);
        while (waitpid(-1, NULL, WNOHANG) > 0) {}

        /* Re-assert root window canvas background */
        XSetWindowBackground(dpy, root, bg_col);
        XClearWindow(dpy, root);
        if (cur != None) {
            XDefineCursor(dpy, root, cur);
            XDefineCursor(dpy, win, cur);
        }

        /* Re-map login window and restore input focus for next login */
        XMapWindow(dpy, win);
        XRaiseWindow(dpy, win);
        XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
        password_buf[0] = '\0';
        focus = FOCUS_PASSWORD;
        is_error = false;
        snprintf(status_msg, sizeof(status_msg), "Session ended cleanly. Enter password to log in.");

        render_login_card(dpy, win, gc, username_buf, password_buf, focus,
                          status_msg, is_error, bg_col, card_col, border_col,
                          cyan_col, indigo_col, text_col, text_dim, input_bg,
                          green_col, red_col, btn_bg, btn_text);
        XFlush(dpy);
    }

    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
