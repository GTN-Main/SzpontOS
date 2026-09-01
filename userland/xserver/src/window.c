/*
 * SzpontOS - SzpontX11 Native X11 Server
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * Window Hierarchy, Tree Traversal, Mapping and Event Delivery
 */

#include "xserver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void window_init_system(void) {
    memset(g_server.windows, 0, sizeof(g_server.windows));

    /* Initialize Root Window (ID = 1) */
    window_t *root = &g_server.root_window;
    memset(root, 0, sizeof(window_t));
    root->id = 1;
    root->x = 0;
    root->y = 0;
    root->width = (uint16_t)g_server.width;
    root->height = (uint16_t)g_server.height;
    root->border_width = 0;
    root->depth = 24;
    root->visual_id = 1;
    root->mapped = true;
    root->background_pixel = 0xFF181825; /* Sleek dark Catppuccin Base */
    root->is_active = true;

    root->backing_pixmap = pixmap_create(NULL, 1, g_server.width, g_server.height, 24);
    if (root->backing_pixmap && root->backing_pixmap->data) {
        for (int i = 0; i < g_server.width * g_server.height; i++) {
            root->backing_pixmap->data[i] = root->background_pixel;
        }
    }

    g_server.focus_window = root;
    g_server.pointer_window = root;
}

window_t *window_create(client_t *c, uint32_t id, window_t *parent, int16_t x, int16_t y,
                        uint16_t w, uint16_t h, uint16_t bw, uint8_t depth, uint32_t visual) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!g_server.windows[i].is_active) {
            window_t *win = &g_server.windows[i];
            memset(win, 0, sizeof(window_t));
            win->id = id;
            win->owner = c;
            win->parent = parent ? parent : &g_server.root_window;
            win->x = x;
            win->y = y;
            win->width = w;
            win->height = h;
            win->border_width = bw;
            win->depth = depth ? depth : 24;
            win->visual_id = visual ? visual : 1;
            win->background_pixel = 0xFFFFFFFF;
            win->border_pixel = 0xFF000000;
            win->mapped = false;
            win->is_active = true;
            win->is_decorated = (win->parent == &g_server.root_window && id != 0x400001 && !win->override_redirect);
            win->is_maximized = false;
            snprintf(win->title, sizeof(win->title), "SzpontOS Application");

            win->backing_pixmap = pixmap_create(c, id, w, h, depth ? depth : 24);
            if (win->backing_pixmap && win->backing_pixmap->data) {
                for (int p = 0; p < w * h; p++) {
                    win->backing_pixmap->data[p] = win->background_pixel;
                }
            }

            /* Link as child of parent (append to end of sibling chain for correct z-order) */
            if (win->parent) {
                if (!win->parent->first_child) {
                    win->parent->first_child = win;
                } else {
                    window_t *sib = win->parent->first_child;
                    while (sib->next_sibling) {
                        sib = sib->next_sibling;
                    }
                    sib->next_sibling = win;
                }
            }

            return win;
        }
    }
    return NULL;
}

window_t *window_find(uint32_t id) {
    if (id == 1 || id == g_server.root_window.id)
        return &g_server.root_window;

    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (g_server.windows[i].is_active && g_server.windows[i].id == id) {
            return &g_server.windows[i];
        }
    }
    return NULL;
}

void window_destroy(window_t *win) {
    if (!win || win == &g_server.root_window)
        return;

    /* Recursively destroy children */
    window_t *child = win->first_child;
    while (child) {
        window_t *next = child->next_sibling;
        window_destroy(child);
        child = next;
    }

    /* Unlink from parent */
    if (win->parent) {
        if (win->parent->first_child == win) {
            win->parent->first_child = win->next_sibling;
        } else {
            window_t *sib = win->parent->first_child;
            while (sib && sib->next_sibling != win) {
                sib = sib->next_sibling;
            }
            if (sib) {
                sib->next_sibling = win->next_sibling;
            }
        }
    }

    /* Free properties */
    property_t *p = win->properties;
    while (p) {
        property_t *next = p->next;
        if (p->data) free(p->data);
        free(p);
        p = next;
    }

    if (win->backing_pixmap) {
        pixmap_destroy(win->backing_pixmap);
        win->backing_pixmap = NULL;
    }

    if (g_server.focus_window == win) {
        g_server.focus_window = &g_server.root_window;
    }
    if (g_server.pointer_window == win) {
        g_server.pointer_window = &g_server.root_window;
    }

    win->is_active = false;
    win->id = 0;
    g_server.needs_redraw = true;
}

void window_map(window_t *win) {
    if (!win || win->mapped)
        return;

    win->mapped = true;
    printf("[SzpontX11] Window 0x%x MAPPED (size %dx%d at %d,%d)!\n", win->id, win->width, win->height, win->x, win->y);
    fflush(stdout);

    /* Send MapNotify */
    x11_map_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = X_MapNotify;
    ev.event = win->id;
    ev.window = win->id;
    ev.override_redirect = win->override_redirect ? 1 : 0;
    window_send_event(win, &ev, StructureNotifyMask);

    /* Send Expose event to client */
    x11_expose_event_t exp;
    memset(&exp, 0, sizeof(exp));
    exp.type = X_Expose;
    exp.window = win->id;
    exp.x = 0;
    exp.y = 0;
    exp.width = win->width;
    exp.height = win->height;
    exp.count = 0;
    window_send_event(win, &exp, ExposureMask);

    if (win->parent && win->parent != &g_server.root_window) {
        if (win->parent->x == 0 && win->parent->y == 0 && win->parent->width <= 1 && win->parent->height <= 1) {
            win->parent->x = 120;
            win->parent->y = 80;
            win->parent->width = win->width;
            win->parent->height = win->height;
        }
        window_raise(win->parent);
    } else if (win->parent == &g_server.root_window && win->x == 0 && win->y == 0 && win->width < g_server.width) {
        if (win->id != 0x400001) {
            win->x = 120;
            win->y = 80;
        }
    }
    window_raise(win);
    g_server.focus_window = win;
    g_server.needs_redraw = true;
}

void window_unmap(window_t *win) {
    if (!win || !win->mapped)
        return;

    win->mapped = false;

    x11_map_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = X_UnmapNotify;
    ev.event = win->id;
    ev.window = win->id;
    ev.override_redirect = win->override_redirect ? 1 : 0;
    window_send_event(win, &ev, StructureNotifyMask);

    if (g_server.focus_window == win) {
        g_server.focus_window = &g_server.root_window;
    }
    g_server.needs_redraw = true;
}

void window_configure(window_t *win, int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t bw) {
    if (!win) return;
    win->x = x;
    win->y = y;
    win->border_width = bw;

    if (w != win->width || h != win->height) {
        win->width = w;
        win->height = h;
        if (win->backing_pixmap) {
            pixmap_destroy(win->backing_pixmap);
            win->backing_pixmap = pixmap_create(win->owner, win->id, w, h, win->depth);
            if (win->backing_pixmap && win->backing_pixmap->data) {
                for (size_t p = 0; p < (size_t)w * h; p++) {
                    win->backing_pixmap->data[p] = win->background_pixel;
                }
            }
        }
    }

    x11_configure_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = X_ConfigureNotify;
    ev.event = win->id;
    ev.window = win->id;
    ev.x = x;
    ev.y = y;
    ev.width = w;
    ev.height = h;
    ev.border_width = bw;
    window_send_event(win, &ev, StructureNotifyMask);

    g_server.needs_redraw = true;
}

void window_set_focus(window_t *win) {
    if (!win) win = &g_server.root_window;
    if (g_server.focus_window == win) return;

    window_t *old_focus = g_server.focus_window;
    g_server.focus_window = win;

    /* Send FocusOut to previous window */
    if (old_focus && old_focus != &g_server.root_window) {
        struct {
            uint8_t  type;
            uint8_t  detail;
            uint16_t sequence_number;
            uint32_t window;
            uint8_t  mode;
            uint8_t  pad[23];
        } __attribute__((packed)) ev_out;
        memset(&ev_out, 0, sizeof(ev_out));
        ev_out.type = X_FocusOut;
        ev_out.detail = 0; /* NotifyAncestor */
        ev_out.window = old_focus->id;
        ev_out.mode = 0;   /* NotifyNormal */
        window_send_event(old_focus, &ev_out, FocusChangeMask);
    }

    /* Send FocusIn to new window */
    if (win && win != &g_server.root_window) {
        struct {
            uint8_t  type;
            uint8_t  detail;
            uint16_t sequence_number;
            uint32_t window;
            uint8_t  mode;
            uint8_t  pad[23];
        } __attribute__((packed)) ev_in;
        memset(&ev_in, 0, sizeof(ev_in));
        ev_in.type = X_FocusIn;
        ev_in.detail = 0; /* NotifyAncestor */
        ev_in.window = win->id;
        ev_in.mode = 0;   /* NotifyNormal */
        window_send_event(win, &ev_in, FocusChangeMask);
    }

    g_server.needs_redraw = true;
}

void window_raise(window_t *win) {
    if (!win || !win->parent || win->parent == win)
        return;
    window_t *parent = win->parent;

    /* Remove from current position in sibling list */
    if (parent->first_child == win) {
        parent->first_child = win->next_sibling;
    } else {
        window_t *prev = parent->first_child;
        while (prev && prev->next_sibling != win) {
            prev = prev->next_sibling;
        }
        if (prev) {
            prev->next_sibling = win->next_sibling;
        }
    }
    win->next_sibling = NULL;

    /* Append to end of sibling chain (drawn last = top of z-order) */
    if (!parent->first_child) {
        parent->first_child = win;
    } else {
        window_t *tail = parent->first_child;
        while (tail->next_sibling) {
            tail = tail->next_sibling;
        }
        tail->next_sibling = win;
    }

    window_set_focus(win);
}

void window_get_absolute_coords(window_t *win, int *abs_x, int *abs_y) {
    int x = 0, y = 0;
    window_t *cur = win;
    while (cur && cur != &g_server.root_window) {
        x += cur->x + cur->border_width;
        y += cur->y + cur->border_width;
        cur = cur->parent;
    }
    if (abs_x) *abs_x = x;
    if (abs_y) *abs_y = y;
}

static window_t *find_child_at(window_t *parent, int px, int py) {
    if (!parent) return NULL;

    /* Check children in reverse order (top-most first) */
    window_t *children[64];
    int count = 0;
    for (window_t *c = parent->first_child; c != NULL && count < 64; c = c->next_sibling) {
        children[count++] = c;
    }

    for (int i = count - 1; i >= 0; i--) {
        window_t *child = children[i];
        window_t *deep = find_child_at(child, px, py);
        if (deep) return deep;

        if (child->mapped) {
            int abs_x, abs_y;
            window_get_absolute_coords(child, &abs_x, &abs_y);
            int top_y = child->is_decorated ? (abs_y - TITLEBAR_HEIGHT) : (abs_y - child->border_width);
            int bot_y = abs_y + child->height + (child->is_decorated ? WINDOW_BORDER_W : (child->border_width * 2));
            int left_x = child->is_decorated ? (abs_x - WINDOW_BORDER_W) : (abs_x - child->border_width);
            int right_x = child->is_decorated ? (abs_x + child->width + WINDOW_BORDER_W) : (abs_x + child->width + child->border_width * 2);

            if (px >= left_x && px < right_x && py >= top_y && py < bot_y) {
                return child;
            }
        }
    }
    return NULL;
}

window_t *window_find_at_pos(int x, int y) {
    window_t *w = find_child_at(&g_server.root_window, x, y);
    return w ? w : &g_server.root_window;
}

void window_send_event(window_t *win, void *event_wire_32bytes, uint32_t event_mask) {
    if (!win || !event_wire_32bytes)
        return;

    /* Deliver to selecting clients or owner */
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_t *c = win->event_clients[i];
        if (c && c->active) {
            if ((win->client_event_masks[i] & event_mask) != 0 || event_mask == 0) {
                if (c->event_count < EVENT_QUEUE_SIZE) {
                    memcpy(c->event_queue[c->event_tail], event_wire_32bytes, 32);
                    uint16_t *seq_ptr = (uint16_t *)((uint8_t *)c->event_queue[c->event_tail] + 2);
                    *seq_ptr = (uint16_t)(c->sequence & 0xFFFF);
                    c->event_tail = (c->event_tail + 1) % EVENT_QUEUE_SIZE;
                    c->event_count++;
                    client_flush_events(c);
                }
            }
        }
    }

    /* If owner is not explicitly registered in event_clients but event_mask matches */
    if (win->owner && win->owner->active && (win->event_mask & event_mask) != 0) {
        bool already_sent = false;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (win->event_clients[i] == win->owner) {
                already_sent = true;
                break;
            }
        }
        if (!already_sent && win->owner->event_count < EVENT_QUEUE_SIZE) {
            memcpy(win->owner->event_queue[win->owner->event_tail], event_wire_32bytes, 32);
            uint16_t *seq_ptr = (uint16_t *)((uint8_t *)win->owner->event_queue[win->owner->event_tail] + 2);
            *seq_ptr = (uint16_t)(win->owner->sequence & 0xFFFF);
            win->owner->event_tail = (win->owner->event_tail + 1) % EVENT_QUEUE_SIZE;
            win->owner->event_count++;
            client_flush_events(win->owner);
        }
    }
}
