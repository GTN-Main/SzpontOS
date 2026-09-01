/*
 * SzpontOS - SzpontX11 Native X11 Server
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * Evdev Input Subsystem (Mouse, Keyboard, Event Generation)
 */

#include "xserver.h"
#include "keysym_defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>

bool input_init(void) {
    g_server.mouse_x = g_server.width / 2;
    g_server.mouse_y = g_server.height / 2;
    g_server.mouse_buttons = 0;
    g_server.shift_pressed = false;
    g_server.caps_locked = false;
    g_server.ctrl_pressed = false;
    g_server.alt_pressed = false;

    g_server.mouse_fd    = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
    g_server.kbd_fd      = open("/dev/input/event1", O_RDONLY | O_NONBLOCK);
    g_server.mice_fd     = -1;
    g_server.psaux_fd    = -1;
    g_server.devmouse_fd = -1;

    /* If evdev event0 is not available, open fallback mouse node */
    if (g_server.mouse_fd < 0) {
        g_server.mice_fd = open("/dev/input/mice", O_RDONLY | O_NONBLOCK);
        if (g_server.mice_fd < 0) {
            g_server.devmouse_fd = open("/dev/mouse", O_RDONLY | O_NONBLOCK);
            if (g_server.devmouse_fd < 0) {
                g_server.psaux_fd = open("/dev/psaux", O_RDONLY | O_NONBLOCK);
            }
        }
    }

    printf("[SzpontX11] Input Devices: Mouse FD %d (fallback mice %d / mouse %d / psaux %d), Keyboard FD %d\n",
           g_server.mouse_fd, g_server.mice_fd, g_server.devmouse_fd, g_server.psaux_fd, g_server.kbd_fd);

    return true;
}

void input_cleanup(void) {
    if (g_server.mouse_fd >= 0)    { close(g_server.mouse_fd);    g_server.mouse_fd = -1; }
    if (g_server.kbd_fd >= 0)      { close(g_server.kbd_fd);      g_server.kbd_fd = -1; }
    if (g_server.mice_fd >= 0)     { close(g_server.mice_fd);     g_server.mice_fd = -1; }
    if (g_server.psaux_fd >= 0)    { close(g_server.psaux_fd);    g_server.psaux_fd = -1; }
    if (g_server.devmouse_fd >= 0) { close(g_server.devmouse_fd); g_server.devmouse_fd = -1; }
}

static void send_motion_event(window_t *win, int root_x, int root_y) {
    if (!win) return;
    int abs_x, abs_y;
    window_get_absolute_coords(win, &abs_x, &abs_y);

    x11_key_button_pointer_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = X_MotionNotify;
    ev.sequence_number = 0;
    ev.time = 0;
    ev.root = 1;
    ev.event = win->id;
    ev.child = 0;
    ev.root_x = (int16_t)root_x;
    ev.root_y = (int16_t)root_y;
    ev.event_x = (int16_t)(root_x - abs_x);
    ev.event_y = (int16_t)(root_y - abs_y);
    ev.state = (uint16_t)g_server.mouse_buttons;
    ev.same_screen = 1;

    window_send_event(win, &ev, PointerMotionMask);
}

static void send_button_event(window_t *win, uint8_t button, bool pressed, int root_x, int root_y) {
    if (!win) return;
    int abs_x, abs_y;
    window_get_absolute_coords(win, &abs_x, &abs_y);

    x11_key_button_pointer_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = pressed ? X_ButtonPress : X_ButtonRelease;
    ev.detail = button;
    ev.sequence_number = 0;
    ev.time = 0;
    ev.root = 1;
    ev.event = win->id;
    ev.child = 0;
    ev.root_x = (int16_t)root_x;
    ev.root_y = (int16_t)root_y;
    ev.event_x = (int16_t)(root_x - abs_x);
    ev.event_y = (int16_t)(root_y - abs_y);
    ev.state = (uint16_t)g_server.mouse_buttons;
    ev.same_screen = 1;

    if (pressed && win && win != &g_server.root_window) {
        window_raise(win);
        if (win->parent && win->parent != &g_server.root_window)
            window_raise(win->parent);
        window_set_focus(win);

        if (win->is_decorated && button == 1) {
            int abs_x, abs_y;
            window_get_absolute_coords(win, &abs_x, &abs_y);
            if (root_y >= abs_y - TITLEBAR_HEIGHT && root_y < abs_y) {
                /* Clicked in Titlebar */
                /* 1. Traffic Light: Close (Red) */
                if (root_x >= abs_x + 8 && root_x <= abs_x + 20) {
                    x11_client_message_event_t cmsg;
                    memset(&cmsg, 0, sizeof(cmsg));
                    cmsg.type = X_ClientMessage;
                    cmsg.format = 32;
                    cmsg.window = win->id;
                    cmsg.message_type = atom_intern("WM_PROTOCOLS", false);
                    cmsg.data.l[0] = atom_intern("WM_DELETE_WINDOW", false);
                    window_send_event(win, &cmsg, 0);
                    return;
                }
                /* 2. Traffic Light: Minimize (Yellow) */
                if (root_x >= abs_x + 24 && root_x <= abs_x + 36) {
                    window_unmap(win);
                    return;
                }
                /* 3. Traffic Light: Maximize / Restore (Green) */
                if (root_x >= abs_x + 40 && root_x <= abs_x + 52) {
                    if (!win->is_maximized) {
                        win->restore_x = win->x;
                        win->restore_y = win->y;
                        win->restore_w = win->width;
                        win->restore_h = win->height;
                        win->x = 0;
                        win->y = 36 + TITLEBAR_HEIGHT;
                        win->width = g_server.width;
                        win->height = g_server.height - 36 - TITLEBAR_HEIGHT;
                        win->is_maximized = true;
                    } else {
                        win->x = win->restore_x;
                        win->y = win->restore_y;
                        win->width = win->restore_w;
                        win->height = win->restore_h;
                        win->is_maximized = false;
                    }
                    g_server.needs_redraw = true;
                    return;
                }
                /* 4. Titlebar Dragging */
                g_server.dragging_window = win;
                g_server.drag_offset_x = root_x - win->x;
                g_server.drag_offset_y = root_y - win->y;
            }
        }
    }

    if (pressed) {
        g_server.grab_window = win;
    } else if (!pressed) {
        if (button == 1) {
            g_server.dragging_window = NULL;
        }
        if (g_server.mouse_buttons == 0) {
            g_server.grab_window = NULL;
        }
    }

    window_send_event(win, &ev, pressed ? ButtonPressMask : ButtonReleaseMask);
}

static void send_key_event(window_t *win, uint8_t keycode, bool pressed) {
    if (!win || win == &g_server.root_window) {
        /* Auto-route keys to topmost active client window */
        for (int w = MAX_WINDOWS - 1; w >= 0; w--) {
            if (g_server.windows[w].is_active && g_server.windows[w].mapped &&
                g_server.windows[w].id != 0x400001 && g_server.windows[w].id != 1) {
                win = &g_server.windows[w];
                break;
            }
        }
    }
    if (!win) win = &g_server.root_window;
    int abs_x, abs_y;
    window_get_absolute_coords(win, &abs_x, &abs_y);

    x11_key_button_pointer_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = pressed ? X_KeyPress : X_KeyRelease;
    ev.detail = keycode;
    ev.sequence_number = 0;
    ev.time = 0;
    ev.root = 1;
    ev.event = win->id;
    ev.child = 0;
    ev.root_x = (int16_t)g_server.mouse_x;
    ev.root_y = (int16_t)g_server.mouse_y;
    ev.event_x = (int16_t)(g_server.mouse_x - abs_x);
    ev.event_y = (int16_t)(g_server.mouse_y - abs_y);
    ev.state = (g_server.shift_pressed ? 1 : 0) | (g_server.caps_locked ? 2 : 0) | (g_server.ctrl_pressed ? 4 : 0) | (g_server.alt_pressed ? 8 : 0);
    ev.same_screen = 1;

    window_send_event(win, &ev, pressed ? KeyPressMask : KeyReleaseMask);
}

static inline int scale_mouse_delta(int delta) {
    if (delta == 0) return 0;
    int abs_d = (delta < 0) ? -delta : delta;
    if (abs_d > 12) return delta * 4;
    if (abs_d > 4)  return delta * 3;
    if (abs_d > 1)  return delta * 2;
    return delta;
}

void input_process_events(void) {
    bool moved = false;

    /* 1. Process evdev mouse events */
    if (g_server.mouse_fd >= 0) {
        struct input_event evs[32];
        ssize_t n = read(g_server.mouse_fd, evs, sizeof(evs));
        if (n > 0) {
            int count = (int)(n / sizeof(struct input_event));
            for (int i = 0; i < count; i++) {
                if (evs[i].type == EV_REL) {
                    if (evs[i].code == REL_X) {
                        g_server.mouse_x += scale_mouse_delta(evs[i].value);
                        moved = true;
                    } else if (evs[i].code == REL_Y) {
                        g_server.mouse_y += scale_mouse_delta(evs[i].value);
                        moved = true;
                    }
                } else if (evs[i].type == EV_KEY) {
                    if (evs[i].code == BTN_LEFT) {
                        send_button_event(window_find_at_pos(g_server.mouse_x, g_server.mouse_y),
                                          1, evs[i].value != 0, g_server.mouse_x, g_server.mouse_y);
                    } else if (evs[i].code == BTN_RIGHT) {
                        send_button_event(window_find_at_pos(g_server.mouse_x, g_server.mouse_y),
                                          3, evs[i].value != 0, g_server.mouse_x, g_server.mouse_y);
                    } else if (evs[i].code == BTN_MIDDLE) {
                        send_button_event(window_find_at_pos(g_server.mouse_x, g_server.mouse_y),
                                          2, evs[i].value != 0, g_server.mouse_x, g_server.mouse_y);
                    }
                }
            }
        }
    }

    /* 2. Process universal /dev/mouse events */
    if (g_server.devmouse_fd >= 0) {
        typedef struct {
            uint8_t buttons;
            int32_t dx;
            int32_t dy;
            int8_t dz;
            int32_t abs_x;
            int32_t abs_y;
            bool is_absolute;
        } u_mouse_event_t;
        u_mouse_event_t uev;
        while (read(g_server.devmouse_fd, &uev, sizeof(uev)) == (ssize_t)sizeof(uev)) {
            if (uev.dx != 0 || uev.dy != 0) {
                g_server.mouse_x += uev.dx;
                g_server.mouse_y += uev.dy;
                moved = true;
            }
            bool btn_l = (uev.buttons & 1) != 0;
            bool btn_r = (uev.buttons & 2) != 0;
            bool btn_m = (uev.buttons & 4) != 0;
            if (btn_l != ((g_server.mouse_buttons & (1 << 8)) != 0)) {
                if (btn_l) g_server.mouse_buttons |= (1 << 8);
                else g_server.mouse_buttons &= ~(1 << 8);
                send_button_event(window_find_at_pos(g_server.mouse_x, g_server.mouse_y), 1, btn_l, g_server.mouse_x, g_server.mouse_y);
            }
            if (btn_r != ((g_server.mouse_buttons & (1 << 10)) != 0)) {
                if (btn_r) g_server.mouse_buttons |= (1 << 10);
                else g_server.mouse_buttons &= ~(1 << 10);
                send_button_event(window_find_at_pos(g_server.mouse_x, g_server.mouse_y), 3, btn_r, g_server.mouse_x, g_server.mouse_y);
            }
            if (btn_m != ((g_server.mouse_buttons & (1 << 9)) != 0)) {
                if (btn_m) g_server.mouse_buttons |= (1 << 9);
                else g_server.mouse_buttons &= ~(1 << 9);
                send_button_event(window_find_at_pos(g_server.mouse_x, g_server.mouse_y), 2, btn_m, g_server.mouse_x, g_server.mouse_y);
            }
        }
    }

    /* 3. Process PS/2 /dev/input/mice or /dev/psaux 3-byte packets */
    int raw_fds[2] = {g_server.mice_fd, g_server.psaux_fd};
    for (int fi = 0; fi < 2; fi++) {
        if (raw_fds[fi] < 0) continue;
        uint8_t packet[4];
        while (read(raw_fds[fi], packet, 3) == 3) {
            int dx = (int)(int8_t)packet[1];
            int dy = (int)(int8_t)packet[2];
            if (dx != 0 || dy != 0) {
                g_server.mouse_x += dx;
                g_server.mouse_y += dy;
                moved = true;
            }

            bool btn_l = (packet[0] & 1) != 0;
            bool btn_r = (packet[0] & 2) != 0;
            bool btn_m = (packet[0] & 4) != 0;

            if (btn_l != ((g_server.mouse_buttons & (1 << 8)) != 0)) {
                if (btn_l) g_server.mouse_buttons |= (1 << 8);
                else g_server.mouse_buttons &= ~(1 << 8);
                send_button_event(window_find_at_pos(g_server.mouse_x, g_server.mouse_y),
                                  1, btn_l, g_server.mouse_x, g_server.mouse_y);
            }
            if (btn_r != ((g_server.mouse_buttons & (1 << 10)) != 0)) {
                if (btn_r) g_server.mouse_buttons |= (1 << 10);
                else g_server.mouse_buttons &= ~(1 << 10);
                send_button_event(window_find_at_pos(g_server.mouse_x, g_server.mouse_y),
                                  3, btn_r, g_server.mouse_x, g_server.mouse_y);
            }
            if (btn_m != ((g_server.mouse_buttons & (1 << 9)) != 0)) {
                if (btn_m) g_server.mouse_buttons |= (1 << 9);
                else g_server.mouse_buttons &= ~(1 << 9);
                send_button_event(window_find_at_pos(g_server.mouse_x, g_server.mouse_y),
                                  2, btn_m, g_server.mouse_x, g_server.mouse_y);
            }
        }
    }

    /* Clamp mouse coordinates to screen boundary */
    if (g_server.mouse_x < 0) g_server.mouse_x = 0;
    if (g_server.mouse_x >= g_server.width) g_server.mouse_x = g_server.width - 1;
    if (g_server.mouse_y < 0) g_server.mouse_y = 0;
    if (g_server.mouse_y >= g_server.height) g_server.mouse_y = g_server.height - 1;

    if (moved) {
        if (g_server.dragging_window) {
            window_t *dwin = g_server.dragging_window;
            int new_x = g_server.mouse_x - g_server.drag_offset_x;
            int new_y = g_server.mouse_y - g_server.drag_offset_y;
            if (new_y < 36 + TITLEBAR_HEIGHT) new_y = 36 + TITLEBAR_HEIGHT;
            dwin->x = (int16_t)new_x;
            dwin->y = (int16_t)new_y;
            g_server.needs_redraw = true;
        }

        window_t *target_win = g_server.grab_window;
        if (!target_win) {
            target_win = window_find_at_pos(g_server.mouse_x, g_server.mouse_y);
        }
        g_server.pointer_window = target_win;
        send_motion_event(target_win, g_server.mouse_x, g_server.mouse_y);
        draw_update_cursor();
    }

    /* 3. Process evdev keyboard events */
    if (g_server.kbd_fd >= 0) {
        struct input_event evs[32];
        ssize_t n = read(g_server.kbd_fd, evs, sizeof(evs));
        if (n > 0) {
            int count = (int)(n / sizeof(struct input_event));
            for (int i = 0; i < count; i++) {
                if (evs[i].type == EV_KEY) {
                    uint16_t code = evs[i].code;
                    bool pressed = (evs[i].value != 0);

                    if (code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT) {
                        g_server.shift_pressed = pressed;
                    } else if (code == KEY_CAPSLOCK && pressed) {
                        g_server.caps_locked = !g_server.caps_locked;
                    } else if (code == KEY_LEFTCTRL || code == KEY_RIGHTCTRL) {
                        g_server.ctrl_pressed = pressed;
                    } else if (code == KEY_LEFTALT || code == KEY_RIGHTALT) {
                        g_server.alt_pressed = pressed;
                    }

                    /* Convert Linux code to X11 keycode (X11 keycode = Linux code + 8) */
                    uint8_t x11_keycode = (uint8_t)(code + 8);
                    send_key_event(g_server.focus_window, x11_keycode, pressed);
                }
            }
        }
    }
}
