/*
 * SzpontOS - SzpontX11 Native X11 Server
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * X11 Core Protocol Request Dispatcher & Handler Implementation
 */

#include "xserver.h"
#include "keysym_defs.h"
#include "font8x16.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/shm.h>

static void apply_window_attributes(window_t *win, client_t *c, uint32_t value_mask, const uint32_t *values) {
    int val_idx = 0;
    if (value_mask & (1 << 0)) { /* CWBackPixmap */ val_idx++; }
    if (value_mask & (1 << 1)) { /* CWBackPixel */ win->background_pixel = values[val_idx++]; }
    if (value_mask & (1 << 2)) { /* CWBorderPixmap */ val_idx++; }
    if (value_mask & (1 << 3)) { /* CWBorderPixel */ win->border_pixel = values[val_idx++]; }
    if (value_mask & (1 << 4)) { /* CWBitGravity */ val_idx++; }
    if (value_mask & (1 << 5)) { /* CWWinGravity */ val_idx++; }
    if (value_mask & (1 << 6)) { /* CWBackingStore */ val_idx++; }
    if (value_mask & (1 << 7)) { /* CWBackingPlanes */ val_idx++; }
    if (value_mask & (1 << 8)) { /* CWBackingPixel */ val_idx++; }
    if (value_mask & (1 << 9)) { /* CWOverrideRedirect */ win->override_redirect = (values[val_idx++] != 0); }
    if (value_mask & (1 << 10)) { /* CWSaveUnder */ val_idx++; }
    if (value_mask & (1 << 11)) { /* CWEventMask */
        uint32_t mask = values[val_idx++];
        win->event_mask |= mask;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!win->event_clients[i] || win->event_clients[i] == c) {
                win->event_clients[i] = c;
                win->client_event_masks[i] = mask;
                break;
            }
        }
    }
    if (value_mask & (1 << 12)) { /* CWDontPropagate */ val_idx++; }
    if (value_mask & (1 << 13)) { /* CWColormap */ val_idx++; }
    if (value_mask & (1 << 14)) { /* CWCursor */ val_idx++; }
}

static void handle_create_window(client_t *c, const uint8_t *req, size_t len) {
    if (len < 32) return;
    uint8_t depth = req[1];
    uint32_t wid = *(const uint32_t *)(req + 4);
    uint32_t parent_id = *(const uint32_t *)(req + 8);
    int16_t x = *(const int16_t *)(req + 12);
    int16_t y = *(const int16_t *)(req + 14);
    uint16_t width = *(const uint16_t *)(req + 16);
    uint16_t height = *(const uint16_t *)(req + 18);
    uint16_t border_width = *(const uint16_t *)(req + 20);
    /* uint16_t class = *(const uint16_t *)(req + 22); */
    uint32_t visual = *(const uint32_t *)(req + 24);
    uint32_t value_mask = *(const uint32_t *)(req + 28);

    window_t *parent = window_find(parent_id);
    window_t *win = window_create(c, wid, parent, x, y, width, height, border_width, depth, visual);
    if (!win) {
        client_send_error(c, BadAlloc, X_CreateWindow, 0, wid);
        return;
    }
    printf("[SzpontX11] Window 0x%x created (parent 0x%x, %dx%d at %d,%d, FD %d)\n", wid, parent_id, width, height, x, y, c->fd);
    fflush(stdout);

    const uint32_t *values = (const uint32_t *)(req + 32);
    apply_window_attributes(win, c, value_mask, values);
}

static void handle_change_window_attributes(client_t *c, const uint8_t *req, size_t len) {
    if (len < 12) return;
    uint32_t wid = *(const uint32_t *)(req + 4);
    uint32_t value_mask = *(const uint32_t *)(req + 8);

    window_t *win = window_find(wid);
    if (!win) {
        client_send_error(c, BadWindow, X_ChangeWindowAttributes, 0, wid);
        return;
    }

    const uint32_t *values = (const uint32_t *)(req + 12);
    apply_window_attributes(win, c, value_mask, values);
}

static void handle_get_window_attributes(client_t *c, const uint8_t *req, size_t len) {
    if (len < 8) return;
    uint32_t wid = *(const uint32_t *)(req + 4);
    window_t *win = window_find(wid);
    if (!win) {
        client_send_error(c, BadWindow, X_GetWindowAttributes, 0, wid);
        return;
    }

    struct {
        x11_reply_header_t hdr;
        uint32_t visual_id;
        uint16_t class_val;
        uint8_t  bit_gravity;
        uint8_t  win_gravity;
        uint32_t backing_planes;
        uint32_t backing_pixel;
        uint8_t  save_under;
        uint8_t  map_is_installed;
        uint8_t  map_state; /* 0 = Unmapped, 2 = Viewable */
        uint8_t  override_redirect;
        uint32_t colormap;
        uint32_t all_event_masks;
        uint32_t your_event_mask;
        uint16_t do_not_propagate_mask;
        uint16_t pad;
    } reply;

    memset(&reply, 0, sizeof(reply));
    reply.hdr.response_type = 1;
    reply.hdr.data = 2; /* BackingStore = Always */
    reply.hdr.length = (sizeof(reply) - 32) / 4;
    reply.visual_id = win->visual_id;
    reply.class_val = InputOutput;
    reply.map_state = win->mapped ? 2 : 0;
    reply.override_redirect = win->override_redirect ? 1 : 0;
    reply.colormap = 1;
    reply.all_event_masks = win->event_mask;
    reply.your_event_mask = win->event_mask;

    client_send_reply(c, &reply, sizeof(reply));
}

static void handle_get_geometry(client_t *c, const uint8_t *req, size_t len) {
    if (len < 8) return;
    uint32_t did = *(const uint32_t *)(req + 4);

    uint8_t depth = 24;
    uint32_t root_id = 1;
    int16_t x = 0, y = 0;
    uint16_t width = 0, height = 0, bw = 0;

    window_t *win = window_find(did);
    if (win) {
        depth = win->depth;
        root_id = 1;
        x = win->x;
        y = win->y;
        width = win->width;
        height = win->height;
        bw = win->border_width;
    } else {
        pixmap_t *pm = pixmap_find(did);
        if (pm) {
            depth = (uint8_t)pm->depth;
            root_id = 1;
            width = (uint16_t)pm->width;
            height = (uint16_t)pm->height;
        } else {
            client_send_error(c, BadDrawable, X_GetGeometry, 0, did);
            return;
        }
    }

    struct {
        x11_reply_header_t hdr;
        uint32_t root;
        int16_t  x;
        int16_t  y;
        uint16_t width;
        uint16_t height;
        uint16_t border_width;
        uint8_t  pad[10];
    } reply;

    memset(&reply, 0, sizeof(reply));
    reply.hdr.response_type = 1;
    reply.hdr.data = depth;
    reply.hdr.length = 0;
    reply.root = root_id;
    reply.x = x;
    reply.y = y;
    reply.width = width;
    reply.height = height;
    reply.border_width = bw;

    client_send_reply(c, &reply, sizeof(reply));
}

static void handle_query_tree(client_t *c, const uint8_t *req, size_t len) {
    if (len < 8) return;
    uint32_t wid = *(const uint32_t *)(req + 4);
    window_t *win = window_find(wid);
    if (!win) {
        client_send_error(c, BadWindow, X_QueryTree, 0, wid);
        return;
    }

    uint32_t children_ids[64];
    uint16_t num_children = 0;
    for (window_t *ch = win->first_child; ch != NULL && num_children < 64; ch = ch->next_sibling) {
        children_ids[num_children++] = ch->id;
    }

    struct {
        x11_reply_header_t hdr;
        uint32_t root;
        uint32_t parent;
        uint16_t num_children;
        uint8_t  pad[14];
    } reply;

    memset(&reply, 0, sizeof(reply));
    reply.hdr.response_type = 1;
    reply.hdr.sequence_number = c->sequence;
    reply.hdr.length = (num_children * 4) / 4;
    reply.root = 1;
    reply.parent = win->parent ? win->parent->id : 0;
    reply.num_children = num_children;

    send(c->fd, &reply, sizeof(reply), 0);
    if (num_children > 0) {
        send(c->fd, children_ids, num_children * sizeof(uint32_t), 0);
    }
}

static void handle_intern_atom(client_t *c, const uint8_t *req, size_t len) {
    if (len < 8) return;
    bool only_if_exists = (req[1] != 0);
    uint16_t name_len = *(const uint16_t *)(req + 4);
    if (len < (size_t)(8 + name_len)) return;

    char name_buf[256] = {0};
    if (name_len > 255) name_len = 255;
    memcpy(name_buf, req + 8, name_len);
    name_buf[name_len] = '\0';

    uint32_t atom_id = atom_intern(name_buf, only_if_exists);

    struct {
        x11_reply_header_t hdr;
        uint32_t atom;
        uint8_t  pad[20];
    } reply;

    memset(&reply, 0, sizeof(reply));
    reply.hdr.response_type = 1;
    reply.atom = atom_id;

    client_send_reply(c, &reply, sizeof(reply));
}

static void handle_get_atom_name(client_t *c, const uint8_t *req, size_t len) {
    if (len < 8) return;
    uint32_t atom_id = *(const uint32_t *)(req + 4);
    const char *name = atom_get_name(atom_id);
    if (!name) {
        client_send_error(c, BadAtom, X_GetAtomName, 0, atom_id);
        return;
    }

    uint16_t name_len = (uint16_t)strlen(name);
    uint16_t pad = (4 - (name_len % 4)) % 4;

    struct {
        x11_reply_header_t hdr;
        uint16_t name_len;
        uint8_t  pad[22];
    } reply;

    memset(&reply, 0, sizeof(reply));
    reply.hdr.response_type = 1;
    reply.hdr.sequence_number = c->sequence;
    reply.hdr.length = (name_len + pad) / 4;
    reply.name_len = name_len;

    send(c->fd, &reply, sizeof(reply), 0);
    send(c->fd, name, name_len, 0);
    if (pad > 0) {
        uint8_t pad_zero[4] = {0};
        send(c->fd, pad_zero, pad, 0);
    }
}

static void handle_change_property(client_t *c, const uint8_t *req, size_t len) {
    if (len < 24) return;
    uint8_t mode = req[1];
    uint32_t wid = *(const uint32_t *)(req + 4);
    uint32_t property = *(const uint32_t *)(req + 8);
    uint32_t type = *(const uint32_t *)(req + 12);
    uint8_t format = req[16];
    uint32_t data_len_units = *(const uint32_t *)(req + 20);

    window_t *win = window_find(wid);
    if (!win) {
        client_send_error(c, BadWindow, X_ChangeProperty, 0, wid);
        return;
    }

    size_t byte_len = 0;
    if (format == 8)  byte_len = data_len_units;
    else if (format == 16) byte_len = data_len_units * 2;
    else if (format == 32) byte_len = data_len_units * 4;

    if (len < 24 + byte_len) return;

    property_set(win, property, type, format, req + 24, byte_len);
}

static void handle_get_property(client_t *c, const uint8_t *req, size_t len) {
    if (len < 24) return;
    uint8_t delete_flag = req[1];
    uint32_t wid = *(const uint32_t *)(req + 4);
    uint32_t property = *(const uint32_t *)(req + 8);
    uint32_t req_type = *(const uint32_t *)(req + 12);
    uint32_t offset = *(const uint32_t *)(req + 16);
    uint32_t length = *(const uint32_t *)(req + 20);

    window_t *win = window_find(wid);
    if (!win) {
        client_send_error(c, BadWindow, X_GetProperty, 0, wid);
        return;
    }

    property_t *prop = property_get(win, property);
    if (!prop) {
        struct {
            x11_reply_header_t hdr;
            uint32_t property_type;
            uint32_t bytes_after;
            uint32_t length;
            uint8_t  pad[12];
        } empty_reply;
        memset(&empty_reply, 0, sizeof(empty_reply));
        empty_reply.hdr.response_type = 1;
        client_send_reply(c, &empty_reply, sizeof(empty_reply));
        return;
    }

    if (req_type != 0 /* AnyPropertyType */ && req_type != prop->type) {
        struct {
            x11_reply_header_t hdr;
            uint32_t property_type;
            uint32_t bytes_after;
            uint32_t value_len;
            uint8_t  pad[12];
        } type_mismatch_reply;
        memset(&type_mismatch_reply, 0, sizeof(type_mismatch_reply));
        type_mismatch_reply.hdr.response_type = 1;
        type_mismatch_reply.hdr.data = prop->format;
        type_mismatch_reply.hdr.sequence_number = c->sequence;
        type_mismatch_reply.hdr.length = 0;
        type_mismatch_reply.property_type = prop->type;
        type_mismatch_reply.bytes_after = (uint32_t)prop->data_len;
        type_mismatch_reply.value_len = 0;
        client_send_reply(c, &type_mismatch_reply, sizeof(type_mismatch_reply));
        return;
    }

    size_t start_byte = (size_t)offset * 4;
    size_t max_bytes = (size_t)length * 4;
    size_t send_bytes = 0;
    size_t bytes_after = 0;

    if (start_byte < prop->data_len) {
        send_bytes = prop->data_len - start_byte;
        if (send_bytes > max_bytes) {
            bytes_after = send_bytes - max_bytes;
            send_bytes = max_bytes;
        }
    }

    uint32_t value_len = (prop->format == 8) ? (uint32_t)send_bytes :
                         (prop->format == 16) ? (uint32_t)(send_bytes / 2) : (uint32_t)(send_bytes / 4);

    uint16_t pad = (4 - (send_bytes % 4)) % 4;

    struct {
        x11_reply_header_t hdr;
        uint32_t property_type;
        uint32_t bytes_after;
        uint32_t value_len;
        uint8_t  pad[12];
    } reply;

    memset(&reply, 0, sizeof(reply));
    reply.hdr.response_type = 1;
    reply.hdr.data = prop->format;
    reply.hdr.sequence_number = c->sequence;
    reply.hdr.length = (send_bytes + pad) / 4;
    reply.property_type = prop->type;
    reply.bytes_after = (uint32_t)bytes_after;
    reply.value_len = value_len;

    send(c->fd, &reply, sizeof(reply), 0);
    if (send_bytes > 0 && prop->data) {
        send(c->fd, (const uint8_t *)prop->data + start_byte, send_bytes, 0);
        if (pad > 0) {
            uint8_t pad_zero[4] = {0};
            send(c->fd, pad_zero, pad, 0);
        }
    }

    if (delete_flag && bytes_after == 0) {
        property_delete(win, property);
    }
}

static void apply_gc_values(gc_t *gc, uint32_t mask, const uint32_t *values) {
    int val_idx = 0;
    if (mask & GCFunction)            { gc->function = (uint8_t)values[val_idx++]; }
    if (mask & GCPlaneMask)           { gc->plane_mask = values[val_idx++]; }
    if (mask & GCForeground)          { gc->foreground = values[val_idx++]; }
    if (mask & GCBackground)          { gc->background = values[val_idx++]; }
    if (mask & GCLineWidth)           { gc->line_width = (uint16_t)values[val_idx++]; }
    if (mask & GCLineStyle)           { val_idx++; }
    if (mask & GCCapStyle)            { val_idx++; }
    if (mask & GCJoinStyle)           { val_idx++; }
    if (mask & GCFillStyle)           { val_idx++; }
    if (mask & GCFillRule)            { val_idx++; }
    if (mask & GCTile)                { val_idx++; }
    if (mask & GCStipple)             { val_idx++; }
    if (mask & GCTileStipXOrigin)     { val_idx++; }
    if (mask & GCTileStipYOrigin)     { val_idx++; }
    if (mask & GCFont)                { val_idx++; }
    if (mask & GCSubwindowMode)       { val_idx++; }
    if (mask & GCGraphicsExposures)   { val_idx++; }
    if (mask & GCClipXOrigin)         { val_idx++; }
    if (mask & GCClipYOrigin)         { val_idx++; }
    if (mask & GCClipMask)            { val_idx++; }
    if (mask & GCDashOffset)          { val_idx++; }
    if (mask & GCDashList)            { val_idx++; }
    if (mask & GCArcMode)             { val_idx++; }
}

static void handle_create_gc(client_t *c, const uint8_t *req, size_t len) {
    if (len < 16) return;
    uint32_t gcid = *(const uint32_t *)(req + 4);
    /* uint32_t drawable = *(const uint32_t *)(req + 8); */
    uint32_t value_mask = *(const uint32_t *)(req + 12);

    gc_t *gc = gc_create(c, gcid);
    if (!gc) {
        client_send_error(c, BadAlloc, X_CreateGC, 0, gcid);
        return;
    }

    const uint32_t *values = (const uint32_t *)(req + 16);
    apply_gc_values(gc, value_mask, values);
}

static void handle_change_gc(client_t *c, const uint8_t *req, size_t len) {
    if (len < 12) return;
    uint32_t gcid = *(const uint32_t *)(req + 4);
    uint32_t value_mask = *(const uint32_t *)(req + 8);

    gc_t *gc = gc_find(gcid);
    if (!gc) {
        client_send_error(c, BadGC, X_ChangeGC, 0, gcid);
        return;
    }

    const uint32_t *values = (const uint32_t *)(req + 12);
    apply_gc_values(gc, value_mask, values);
}

static void handle_create_pixmap(client_t *c, const uint8_t *req, size_t len) {
    if (len < 16) return;
    uint8_t depth = req[1];
    uint32_t pid = *(const uint32_t *)(req + 4);
    /* uint32_t drawable = *(const uint32_t *)(req + 8); */
    uint16_t width = *(const uint16_t *)(req + 12);
    uint16_t height = *(const uint16_t *)(req + 14);

    pixmap_t *pm = pixmap_create(c, pid, width, height, depth);
    if (!pm) {
        client_send_error(c, BadAlloc, X_CreatePixmap, 0, pid);
    }
}

static uint32_t *get_drawable_buffer(uint32_t id, int *w, int *h, int *pitch) {
    window_t *win = window_find(id);
    if (win) {
        if (!win->backing_pixmap) {
            win->backing_pixmap = pixmap_create(win->owner, win->id, win->width, win->height, win->depth);
        }
        if (win->backing_pixmap) {
            if (w) *w = win->width;
            if (h) *h = win->height;
            if (pitch) *pitch = win->backing_pixmap->pitch;
            return win->backing_pixmap->data;
        }
    }
    pixmap_t *pm = pixmap_find(id);
    if (pm) {
        if (w) *w = pm->width;
        if (h) *h = pm->height;
        if (pitch) *pitch = pm->pitch;
        return pm->data;
    }
    return NULL;
}

static void handle_poly_fill_rectangle(client_t *c, const uint8_t *req, size_t len) {
    if (len < 12) return;
    uint32_t did = *(const uint32_t *)(req + 4);
    uint32_t gcid = *(const uint32_t *)(req + 8);

    gc_t *gc = gc_find(gcid);
    if (!gc) { client_send_error(c, BadGC, X_PolyFillRectangle, 0, gcid); return; }

    int dw = 0, dh = 0, dpitch = 0;
    uint32_t *buf = get_drawable_buffer(did, &dw, &dh, &dpitch);
    if (!buf) { client_send_error(c, BadDrawable, X_PolyFillRectangle, 0, did); return; }

    const struct {
        int16_t  x, y;
        uint16_t width, height;
    } *rects = (const void *)(req + 12);

    int count = (int)((len - 12) / 8);
    for (int i = 0; i < count; i++) {
        draw_fill_rect(buf, dpitch, dw, dh, rects[i].x, rects[i].y, rects[i].width, rects[i].height,
                       gc->foreground, gc->function);
    }
}

static void handle_poly_rectangle(client_t *c, const uint8_t *req, size_t len) {
    if (len < 12) return;
    uint32_t did = *(const uint32_t *)(req + 4);
    uint32_t gcid = *(const uint32_t *)(req + 8);

    gc_t *gc = gc_find(gcid);
    if (!gc) { client_send_error(c, BadGC, X_PolyRectangle, 0, gcid); return; }

    int dw = 0, dh = 0, dpitch = 0;
    uint32_t *buf = get_drawable_buffer(did, &dw, &dh, &dpitch);
    if (!buf) { client_send_error(c, BadDrawable, X_PolyRectangle, 0, did); return; }

    const struct {
        int16_t  x, y;
        uint16_t width, height;
    } *rects = (const void *)(req + 12);

    int count = (int)((len - 12) / 8);
    for (int i = 0; i < count; i++) {
        draw_rect(buf, dpitch, dw, dh, rects[i].x, rects[i].y, rects[i].width, rects[i].height,
                  gc->foreground, gc->line_width);
    }
}

static void handle_poly_line(client_t *c, const uint8_t *req, size_t len) {
    if (len < 16) return;
    uint8_t mode = req[1]; /* 0 = CoordModeOrigin, 1 = CoordModePrevious */
    uint32_t did = *(const uint32_t *)(req + 4);
    uint32_t gcid = *(const uint32_t *)(req + 8);

    gc_t *gc = gc_find(gcid);
    if (!gc) { client_send_error(c, BadGC, X_PolyLine, 0, gcid); return; }

    int dw = 0, dh = 0, dpitch = 0;
    uint32_t *buf = get_drawable_buffer(did, &dw, &dh, &dpitch);
    if (!buf) { client_send_error(c, BadDrawable, X_PolyLine, 0, did); return; }

    const struct {
        int16_t x, y;
    } *pts = (const void *)(req + 12);

    int count = (int)((len - 12) / 4);
    int cur_x = 0, cur_y = 0;

    for (int i = 0; i < count - 1; i++) {
        int x1, y1, x2, y2;
        if (mode == 0) {
            x1 = pts[i].x;   y1 = pts[i].y;
            x2 = pts[i+1].x; y2 = pts[i+1].y;
        } else {
            if (i == 0) { cur_x = pts[0].x; cur_y = pts[0].y; }
            x1 = cur_x; y1 = cur_y;
            cur_x += pts[i+1].x; cur_y += pts[i+1].y;
            x2 = cur_x; y2 = cur_y;
        }
        draw_line(buf, dpitch, dw, dh, x1, y1, x2, y2, gc->foreground, gc->line_width);
    }
}

static void handle_poly_arc(client_t *c, const uint8_t *req, size_t len, bool fill) {
    if (len < 12) return;
    uint32_t did = *(const uint32_t *)(req + 4);
    uint32_t gcid = *(const uint32_t *)(req + 8);

    gc_t *gc = gc_find(gcid);
    if (!gc) return;

    int dw = 0, dh = 0, dpitch = 0;
    uint32_t *buf = get_drawable_buffer(did, &dw, &dh, &dpitch);
    if (!buf) return;

    const struct {
        int16_t  x, y;
        uint16_t width, height;
        int16_t  angle1, angle2;
    } *arcs = (const void *)(req + 12);

    int count = (int)((len - 12) / 12);
    for (int i = 0; i < count; i++) {
        draw_arc(buf, dpitch, dw, dh, arcs[i].x, arcs[i].y, arcs[i].width, arcs[i].height,
                 arcs[i].angle1, arcs[i].angle2, gc->foreground, fill);
    }
}

static void handle_image_text8(client_t *c, const uint8_t *req, size_t len) {
    if (len < 16) return;
    uint8_t str_len = req[1];
    uint32_t did = *(const uint32_t *)(req + 4);
    uint32_t gcid = *(const uint32_t *)(req + 8);
    int16_t x = *(const int16_t *)(req + 12);
    int16_t y = *(const int16_t *)(req + 14);

    gc_t *gc = gc_find(gcid);
    if (!gc) return;

    int dw = 0, dh = 0, dpitch = 0;
    uint32_t *buf = get_drawable_buffer(did, &dw, &dh, &dpitch);
    if (!buf) return;

    const char *text = (const char *)(req + 16);
    draw_text(buf, dpitch, dw, dh, x, y - FONT_ASCENT, text, str_len, gc->foreground, gc->background, false);
}

static void handle_poly_text8(client_t *c, const uint8_t *req, size_t len) {
    if (len < 16) return;
    uint32_t did = *(const uint32_t *)(req + 4);
    uint32_t gcid = *(const uint32_t *)(req + 8);
    int16_t x = *(const int16_t *)(req + 12);
    int16_t y = *(const int16_t *)(req + 14);

    gc_t *gc = gc_find(gcid);
    if (!gc) return;

    int dw = 0, dh = 0, dpitch = 0;
    uint32_t *buf = get_drawable_buffer(did, &dw, &dh, &dpitch);
    if (!buf) return;

    const uint8_t *p = req + 16;
    const uint8_t *end = req + len;

    while (p < end) {
        uint8_t item_len = *p++;
        if (item_len == 0) break;
        if (item_len == 255) {
            /* Font shift */
            p += 4;
            continue;
        }
        int8_t delta = (int8_t)*p++;
        x += delta;
        draw_text(buf, dpitch, dw, dh, x, y - FONT_ASCENT, (const char *)p, item_len, gc->foreground, gc->background, true);
        x += item_len * FONT_WIDTH;
        p += item_len;
    }
}

static void handle_alloc_color(client_t *c, const uint8_t *req, size_t len) {
    if (len < 16) return;
    /* uint32_t cmap = *(const uint32_t *)(req + 4); */
    uint16_t red   = *(const uint16_t *)(req + 8);
    uint16_t green = *(const uint16_t *)(req + 10);
    uint16_t blue  = *(const uint16_t *)(req + 12);

    uint32_t pixel = 0xFF000000 | (((uint32_t)(red >> 8)) << 16) |
                                  (((uint32_t)(green >> 8)) << 8) |
                                  ((uint32_t)(blue >> 8));

    struct {
        x11_reply_header_t hdr;
        uint16_t red;
        uint16_t green;
        uint16_t blue;
        uint16_t pad1;
        uint32_t pixel;
        uint8_t  pad2[12];
    } reply;

    memset(&reply, 0, sizeof(reply));
    reply.hdr.response_type = 1;
    reply.red = red;
    reply.green = green;
    reply.blue = blue;
    reply.pixel = pixel;

    client_send_reply(c, &reply, sizeof(reply));
}

static void handle_alloc_named_color(client_t *c, const uint8_t *req, size_t len) {
    if (len < 12) return;
    uint16_t name_len = *(const uint16_t *)(req + 8);
    char name[64] = {0};
    if (name_len > 63) name_len = 63;
    memcpy(name, req + 12, name_len);

    uint32_t pixel = 0xFFFFFFFF;
    uint16_t r = 0xFFFF, g = 0xFFFF, b = 0xFFFF;

    if (strcasecmp(name, "black") == 0) { pixel = 0xFF000000; r = g = b = 0; }
    else if (strcasecmp(name, "white") == 0) { pixel = 0xFFFFFFFF; r = g = b = 0xFFFF; }
    else if (strcasecmp(name, "red") == 0)   { pixel = 0xFFFF0000; r = 0xFFFF; g = b = 0; }
    else if (strcasecmp(name, "green") == 0) { pixel = 0xFF00FF00; g = 0xFFFF; r = b = 0; }
    else if (strcasecmp(name, "blue") == 0)  { pixel = 0xFF0000FF; b = 0xFFFF; r = g = 0; }
    else if (strcasecmp(name, "cyan") == 0)  { pixel = 0xFF00FFFF; g = b = 0xFFFF; r = 0; }
    else if (strcasecmp(name, "yellow") == 0){ pixel = 0xFFFFFF00; r = g = 0xFFFF; b = 0; }

    struct {
        x11_reply_header_t hdr;
        uint32_t pixel;
        uint16_t exact_red, exact_green, exact_blue;
        uint16_t screen_red, screen_green, screen_blue;
        uint8_t  pad[8];
    } reply;

    memset(&reply, 0, sizeof(reply));
    reply.hdr.response_type = 1;
    reply.pixel = pixel;
    reply.exact_red = reply.screen_red = r;
    reply.exact_green = reply.screen_green = g;
    reply.exact_blue = reply.screen_blue = b;

    client_send_reply(c, &reply, sizeof(reply));
}

static void handle_query_colors(client_t *c, const uint8_t *req, size_t len) {
    if (len < 8) return;
    size_t num_pixels = (len - 8) / 4;
    const uint32_t *pixels = (const uint32_t *)(req + 8);

    struct {
        uint8_t  response_type;
        uint8_t  pad1;
        uint16_t sequence_number;
        uint32_t length;
        uint16_t ncolors;
        uint16_t pad2;
        uint8_t  pad[20];
    } __attribute__((packed)) reply;

    memset(&reply, 0, sizeof(reply));
    reply.response_type = 1;
    reply.sequence_number = c->sequence;
    reply.length = (uint32_t)(num_pixels * 2);
    reply.ncolors = (uint16_t)num_pixels;

    struct {
        uint16_t red;
        uint16_t green;
        uint16_t blue;
        uint16_t pad;
    } __attribute__((packed)) rgbs[64];

    if (num_pixels > 64) num_pixels = 64;

    for (size_t i = 0; i < num_pixels; i++) {
        uint32_t px = pixels[i];
        rgbs[i].red = (uint16_t)(((px >> 16) & 0xFF) * 257);
        rgbs[i].green = (uint16_t)(((px >> 8) & 0xFF) * 257);
        rgbs[i].blue = (uint16_t)((px & 0xFF) * 257);
        rgbs[i].pad = 0;
    }

    send(c->fd, &reply, sizeof(reply), 0);
    if (num_pixels > 0) {
        send(c->fd, rgbs, num_pixels * sizeof(rgbs[0]), 0);
    }
}

static void handle_lookup_color(client_t *c, const uint8_t *req, size_t len) {
    if (len < 12) return;
    uint16_t name_len = *(const uint16_t *)(req + 8);
    char name[64] = {0};
    if (name_len > 63) name_len = 63;
    memcpy(name, req + 12, name_len);

    uint16_t r = 0xFFFF, g = 0xFFFF, b = 0xFFFF;
    if (strcasecmp(name, "black") == 0) { r = g = b = 0; }
    else if (strcasecmp(name, "white") == 0) { r = g = b = 0xFFFF; }
    else if (strcasecmp(name, "red") == 0)   { r = 0xFFFF; g = b = 0; }
    else if (strcasecmp(name, "green") == 0) { g = 0xFFFF; r = b = 0; }
    else if (strcasecmp(name, "blue") == 0)  { b = 0xFFFF; r = g = 0; }
    else if (strcasecmp(name, "cyan") == 0)  { g = b = 0xFFFF; r = 0; }
    else if (strcasecmp(name, "yellow") == 0){ r = g = 0xFFFF; b = 0; }

    struct {
        x11_reply_header_t hdr;
        uint16_t exact_red, exact_green, exact_blue;
        uint16_t screen_red, screen_green, screen_blue;
        uint8_t  pad[12];
    } reply;

    memset(&reply, 0, sizeof(reply));
    reply.hdr.response_type = 1;
    reply.exact_red = reply.screen_red = r;
    reply.exact_green = reply.screen_green = g;
    reply.exact_blue = reply.screen_blue = b;

    client_send_reply(c, &reply, sizeof(reply));
}

static void handle_get_keyboard_mapping(client_t *c, const uint8_t *req, size_t len) {
    if (len < 8) return;
    uint8_t first_keycode = req[4];
    uint8_t count = req[5];
    if (first_keycode < 8) first_keycode = 8;

    uint32_t keysyms[256 * 2];
    int total_keysyms = count * 2;

    for (int i = 0; i < count; i++) {
        uint8_t kc = first_keycode + i;
        uint16_t evdev_code = (kc >= 8) ? (kc - 8) : 0;
        keysyms[i * 2 + 0] = evdev_to_keysym(evdev_code, false, false);
        keysyms[i * 2 + 1] = evdev_to_keysym(evdev_code, true, false);
    }

    struct {
        x11_reply_header_t hdr;
        uint8_t pad[24];
    } reply;

    memset(&reply, 0, sizeof(reply));
    reply.hdr.response_type = 1;
    reply.hdr.data = 2; /* 2 keysyms per keycode */
    reply.hdr.sequence_number = c->sequence;
    reply.hdr.length = (total_keysyms * 4) / 4;

    send(c->fd, &reply, sizeof(reply), 0);
    send(c->fd, keysyms, total_keysyms * sizeof(uint32_t), 0);
}

static void handle_get_modifier_mapping(client_t *c) {
    uint8_t mod_map[8 * 2] = {0};
    mod_map[0] = 50; /* Shift_L (evdev 42 + 8) */
    mod_map[1] = 62; /* Shift_R (evdev 54 + 8) */
    mod_map[4] = 37; /* Control_L (evdev 29 + 8) */
    mod_map[5] = 105;/* Control_R (evdev 97 + 8) */
    mod_map[6] = 64; /* Alt_L (evdev 56 + 8) */

    struct {
        x11_reply_header_t hdr;
        uint8_t pad[24];
    } reply;

    memset(&reply, 0, sizeof(reply));
    reply.hdr.response_type = 1;
    reply.hdr.data = 2; /* 2 keycodes per modifier */
    reply.hdr.sequence_number = c->sequence;
    reply.hdr.length = sizeof(mod_map) / 4;

    send(c->fd, &reply, sizeof(reply), 0);
    send(c->fd, mod_map, sizeof(mod_map), 0);
}

static void handle_query_pointer(client_t *c, const uint8_t *req, size_t len) {
    if (len < 8) return;
    uint32_t wid = *(const uint32_t *)(req + 4);
    window_t *win = window_find(wid);
    if (!win) win = &g_server.root_window;

    int abs_x, abs_y;
    window_get_absolute_coords(win, &abs_x, &abs_y);

    struct {
        x11_reply_header_t hdr;
        uint32_t root;
        uint32_t child;
        int16_t  root_x, root_y;
        int16_t  win_x, win_y;
        uint16_t mask;
        uint8_t  same_screen;
        uint8_t  pad[5];
    } reply;

    memset(&reply, 0, sizeof(reply));
    reply.hdr.response_type = 1;
    reply.hdr.data = 1;
    reply.root = 1;
    reply.child = g_server.pointer_window ? g_server.pointer_window->id : 0;
    reply.root_x = (int16_t)g_server.mouse_x;
    reply.root_y = (int16_t)g_server.mouse_y;
    reply.win_x = (int16_t)(g_server.mouse_x - abs_x);
    reply.win_y = (int16_t)(g_server.mouse_y - abs_y);
    reply.mask = (uint16_t)g_server.mouse_buttons;
    reply.same_screen = 1;

    client_send_reply(c, &reply, sizeof(reply));
}

static void handle_put_image(client_t *c, const uint8_t *req, size_t len) {
    (void)c;
    if (len < 24) return;
    uint8_t format = req[1];
    uint32_t drawable_id = *(const uint32_t *)(req + 4);
    uint16_t width = *(const uint16_t *)(req + 12);
    uint16_t height = *(const uint16_t *)(req + 14);
    int16_t dst_x = *(const int16_t *)(req + 16);
    int16_t dst_y = *(const int16_t *)(req + 18);

    if (width == 0 || height == 0) return;

    pixmap_t *pix = NULL;
    window_t *win = window_find(drawable_id);
    if (win) {
        pix = win->backing_pixmap;
    } else {
        pix = pixmap_find(drawable_id);
    }
    if (!pix || !pix->data) return;

    const uint8_t *data = req + 24;

    /* Handle ZPixmap (32bpp BGRX / RGBX) */
    if (format == 2 /* ZPixmap */) {
        const uint32_t *src_pixels = (const uint32_t *)data;
        int src_pitch = (int)width * 4;
        draw_blit(pix->data, pix->pitch, pix->width, pix->height,
                  dst_x, dst_y, src_pixels, src_pitch, width, height,
                  0, 0, width, height);
        if (win) {
            g_server.needs_redraw = true;
        }
    }
}

/* ==============================================================================
 * MIT-SHM Extension Implementation (Zero-Copy Shared Memory Support)
 * ============================================================================== */

#define X_Shm_Opcode 130
#define X_ShmQueryVersion 0
#define X_ShmAttach 1
#define X_ShmDetach 2
#define X_ShmPutImage 3
#define X_ShmGetImage 4
#define X_ShmCreatePixmap 5

#define ShmCompletionEvent 64
#define BadShmSeg 128

shmseg_t *shm_find_seg(client_t *c, uint32_t seg_id) {
    if (!c || seg_id == 0) return NULL;
    for (int i = 0; i < MAX_SHMSEGS; i++) {
        if (c->shm_segments[i].active && c->shm_segments[i].seg_id == seg_id) {
            return &c->shm_segments[i];
        }
    }
    return NULL;
}

void shm_cleanup_client(client_t *c) {
    if (!c) return;
    for (int i = 0; i < MAX_SHMSEGS; i++) {
        if (c->shm_segments[i].active) {
            if (c->shm_segments[i].mapped_addr) {
                shmdt(c->shm_segments[i].mapped_addr);
            }
            memset(&c->shm_segments[i], 0, sizeof(shmseg_t));
        }
    }
}

static void handle_shm_query_version(client_t *c) {
    struct {
        uint8_t  type;
        uint8_t  shared_pixmaps;
        uint16_t sequence_number;
        uint32_t length;
        uint16_t major_version;
        uint16_t minor_version;
        uint16_t uid;
        uint16_t gid;
        uint8_t  pixmap_format;
        uint8_t  pad0;
        uint16_t pad1;
        uint8_t  pad2[12];
    } __attribute__((packed)) reply;

    memset(&reply, 0, sizeof(reply));
    reply.type = 1;
    reply.shared_pixmaps = 1;
    reply.sequence_number = c->sequence;
    reply.length = 0;
    reply.major_version = 1;
    reply.minor_version = 2;
    reply.uid = 0;
    reply.gid = 0;
    reply.pixmap_format = 2; /* ZPixmap */
    client_send_reply(c, &reply, sizeof(reply));
}

static void handle_shm_attach(client_t *c, const uint8_t *req, size_t len) {
    if (len < 16) return;
    uint32_t shmseg_id = *(const uint32_t *)(req + 4);
    int shmid = *(const int *)(req + 8);
    bool read_only = (req[12] != 0);

    /* Attach shared memory in X server */
    void *mapped = shmat(shmid, NULL, read_only ? SHM_RDONLY : 0);
    if (mapped == (void *)-1) {
        printf("[SzpontX11] MIT-SHM: shmat failed for SHMID %d\n", shmid);
        client_send_error(c, BadAlloc, X_Shm_Opcode, X_ShmAttach, shmseg_id);
        return;
    }

    shmseg_t *seg = NULL;
    for (int i = 0; i < MAX_SHMSEGS; i++) {
        if (!c->shm_segments[i].active) {
            seg = &c->shm_segments[i];
            break;
        }
    }

    if (!seg) {
        shmdt(mapped);
        client_send_error(c, BadAlloc, X_Shm_Opcode, X_ShmAttach, shmseg_id);
        return;
    }

    seg->seg_id = shmseg_id;
    seg->shmid = shmid;
    seg->mapped_addr = mapped;
    seg->read_only = read_only;
    seg->active = true;
    printf("[SzpontX11] MIT-SHM: Attached SegID 0x%x -> SHMID %d at %p (Zero-Copy Active)\n",
           shmseg_id, shmid, mapped);
}

static void handle_shm_detach(client_t *c, const uint8_t *req, size_t len) {
    if (len < 8) return;
    uint32_t shmseg_id = *(const uint32_t *)(req + 4);
    shmseg_t *seg = shm_find_seg(c, shmseg_id);
    if (seg) {
        if (seg->mapped_addr) {
            shmdt(seg->mapped_addr);
        }
        memset(seg, 0, sizeof(shmseg_t));
        printf("[SzpontX11] MIT-SHM: Detached SegID 0x%x\n", shmseg_id);
    }
}

static void handle_shm_put_image(client_t *c, const uint8_t *req, size_t len) {
    if (len < 40) return;
    uint32_t drawable_id = *(const uint32_t *)(req + 4);
    /* uint32_t gc_id = *(const uint32_t *)(req + 8); */
    uint16_t total_w = *(const uint16_t *)(req + 12);
    uint16_t total_h = *(const uint16_t *)(req + 14);
    uint16_t src_x = *(const uint16_t *)(req + 16);
    uint16_t src_y = *(const uint16_t *)(req + 18);
    uint16_t src_w = *(const uint16_t *)(req + 20);
    uint16_t src_h = *(const uint16_t *)(req + 22);
    int16_t dst_x = *(const int16_t *)(req + 24);
    int16_t dst_y = *(const int16_t *)(req + 26);
    /* uint8_t depth = req[28]; */
    /* uint8_t format = req[29]; */
    uint8_t send_event = req[30];
    uint32_t shmseg_id = *(const uint32_t *)(req + 32);
    uint32_t offset = *(const uint32_t *)(req + 36);

    shmseg_t *seg = shm_find_seg(c, shmseg_id);
    if (!seg || !seg->mapped_addr) {
        client_send_error(c, BadShmSeg, X_Shm_Opcode, X_ShmPutImage, shmseg_id);
        return;
    }

    pixmap_t *dst_pix = NULL;
    window_t *win = window_find(drawable_id);
    if (win) {
        dst_pix = win->backing_pixmap;
    } else {
        dst_pix = pixmap_find(drawable_id);
    }

    if (dst_pix && dst_pix->data) {
        const uint32_t *src_pixels = (const uint32_t *)((const uint8_t *)seg->mapped_addr + offset);
        int src_pitch = (int)total_w * 4;

        /* Zero-Copy Blit directly from shared memory into backing pixmap */
        draw_blit(dst_pix->data, dst_pix->pitch, dst_pix->width, dst_pix->height,
                  dst_x, dst_y, src_pixels, src_pitch, total_w, total_h,
                  src_x, src_y, src_w, src_h);

        if (win) {
            g_server.needs_redraw = true;
        }
    }

    if (send_event) {
        struct {
            uint8_t type;
            uint8_t pad;
            uint16_t sequence;
            uint32_t drawable;
            uint16_t minor_event;
            uint8_t major_event;
            uint8_t pad2;
            uint32_t shmseg;
            uint32_t offset;
            uint8_t pad3[12];
        } ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = ShmCompletionEvent;
        ev.sequence = c->sequence;
        ev.drawable = drawable_id;
        ev.minor_event = X_ShmPutImage;
        ev.major_event = X_Shm_Opcode;
        ev.shmseg = shmseg_id;
        ev.offset = offset;
        client_send_reply(c, &ev, sizeof(ev));
    }
}

static void handle_shm_create_pixmap(client_t *c, const uint8_t *req, size_t len) {
    if (len < 28) return;
    uint32_t pid = *(const uint32_t *)(req + 4);
    /* uint32_t drawable = *(const uint32_t *)(req + 8); */
    uint16_t w = *(const uint16_t *)(req + 12);
    uint16_t h = *(const uint16_t *)(req + 14);
    uint8_t depth = req[16];
    uint32_t shmseg_id = *(const uint32_t *)(req + 20);
    uint32_t offset = *(const uint32_t *)(req + 24);

    shmseg_t *seg = shm_find_seg(c, shmseg_id);
    if (!seg || !seg->mapped_addr) {
        client_send_error(c, BadShmSeg, X_Shm_Opcode, X_ShmCreatePixmap, shmseg_id);
        return;
    }

    pixmap_t *pm = pixmap_create(c, pid, w, h, depth);
    if (pm) {
        if (pm->data) {
            free(pm->data);
        }
        pm->data = (uint32_t *)((uint8_t *)seg->mapped_addr + offset);
        pm->pitch = w * 4;
        printf("[SzpontX11] MIT-SHM: Created Shared Pixmap 0x%x (%dx%d, Seg 0x%x offset %u)\n",
               pid, w, h, shmseg_id, offset);
    }
}

static void handle_copy_area(client_t *c, const uint8_t *req, size_t len) {
    (void)c;
    if (len < 28) return;
    uint32_t src_id = *(const uint32_t *)(req + 4);
    uint32_t dst_id = *(const uint32_t *)(req + 8);
    int16_t src_x = *(const int16_t *)(req + 16);
    int16_t src_y = *(const int16_t *)(req + 18);
    int16_t dst_x = *(const int16_t *)(req + 20);
    int16_t dst_y = *(const int16_t *)(req + 22);
    uint16_t width = *(const uint16_t *)(req + 24);
    uint16_t height = *(const uint16_t *)(req + 26);

    pixmap_t *src_pix = NULL;
    window_t *src_win = window_find(src_id);
    if (src_win) src_pix = src_win->backing_pixmap;
    else src_pix = pixmap_find(src_id);

    pixmap_t *dst_pix = NULL;
    window_t *dst_win = window_find(dst_id);
    if (dst_win) dst_pix = dst_win->backing_pixmap;
    else dst_pix = pixmap_find(dst_id);

    if (!src_pix || !dst_pix || !src_pix->data || !dst_pix->data) return;

    draw_blit(dst_pix->data, dst_pix->pitch, dst_pix->width, dst_pix->height, dst_x, dst_y,
              src_pix->data, src_pix->pitch, src_pix->width, src_pix->height,
              src_x, src_y, width, height);
}

static void handle_configure_window(client_t *c, const uint8_t *req, size_t len) {
    if (len < 12) return;
    uint32_t wid = *(const uint32_t *)(req + 4);
    uint16_t value_mask = *(const uint16_t *)(req + 8);

    window_t *win = window_find(wid);
    if (!win) {
        client_send_error(c, BadWindow, X_ConfigureWindow, 0, wid);
        return;
    }

    const uint32_t *values = (const uint32_t *)(req + 12);
    int val_idx = 0;

    int16_t new_x = win->x;
    int16_t new_y = win->y;
    uint16_t new_w = win->width;
    uint16_t new_h = win->height;
    uint16_t new_bw = win->border_width;

    if (value_mask & (1 << 0)) { /* CWX */ new_x = (int16_t)values[val_idx++]; }
    if (value_mask & (1 << 1)) { /* CWY */ new_y = (int16_t)values[val_idx++]; }
    if (value_mask & (1 << 2)) { /* CWWidth */ new_w = (uint16_t)values[val_idx++]; }
    if (value_mask & (1 << 3)) { /* CWHeight */ new_h = (uint16_t)values[val_idx++]; }
    if (value_mask & (1 << 4)) { /* CWBorderWidth */ new_bw = (uint16_t)values[val_idx++]; }
    if (value_mask & (1 << 5)) { /* CWSibling */ val_idx++; }
    if (value_mask & (1 << 6)) { /* CWStackMode */
        uint32_t stack_mode = values[val_idx++];
        if (stack_mode == 0 /* Above */) {
            window_raise(win);
        }
    }

    window_configure(win, new_x, new_y, new_w, new_h, new_bw);
}

static void handle_open_font(client_t *c, const uint8_t *req, size_t len) {
    (void)c; (void)req; (void)len;
}

static void handle_close_font(client_t *c, const uint8_t *req, size_t len) {
    (void)c; (void)req; (void)len;
}

static void handle_query_font(client_t *c, const uint8_t *req, size_t len) {
    (void)req; (void)len;
    printf("[SzpontX11] handle_query_font called for FD %d!\n", c->fd);
    fflush(stdout);
    struct {
        uint8_t type;
        uint8_t pad1;
        uint16_t sequence_number;
        uint32_t length;
        int16_t min_lb;
        int16_t min_rb;
        int16_t min_width;
        int16_t min_ascent;
        int16_t min_descent;
        uint16_t min_attr;
        uint32_t pad2;
        int16_t max_lb;
        int16_t max_rb;
        int16_t max_width;
        int16_t max_ascent;
        int16_t max_descent;
        uint16_t max_attr;
        uint32_t pad3;
        uint16_t min_char;
        uint16_t max_char;
        uint16_t default_char;
        uint16_t nfont_props;
        uint8_t draw_direction;
        uint8_t min_byte1;
        uint8_t max_byte1;
        uint8_t all_chars_exist;
        int16_t font_ascent;
        int16_t font_descent;
        uint32_t nchar_infos;
    } __attribute__((packed)) reply;

    memset(&reply, 0, sizeof(reply));
    reply.type = 1;
    reply.sequence_number = c->sequence;
    reply.length = (sizeof(reply) - 32) / 4;

    reply.min_lb = 0;
    reply.min_rb = 8;
    reply.min_width = 8;
    reply.min_ascent = 13;
    reply.min_descent = 3;

    reply.max_lb = 0;
    reply.max_rb = 8;
    reply.max_width = 8;
    reply.max_ascent = 13;
    reply.max_descent = 3;

    reply.min_char = 0;
    reply.max_char = 255;
    reply.default_char = 32;
    reply.nfont_props = 0;
    reply.draw_direction = 0;
    reply.all_chars_exist = 1;
    reply.font_ascent = 13;
    reply.font_descent = 3;
    reply.nchar_infos = 0;

    client_send_reply(c, &reply, sizeof(reply));
}

static void handle_query_text_extents(client_t *c, const uint8_t *req, size_t len) {
    if (len < 8) return;
    size_t num_chars = (len - 8);
    int odd = req[1];
    if (odd && num_chars > 0) num_chars--;

    struct {
        uint8_t type;
        uint8_t draw_direction;
        uint16_t sequence_number;
        uint32_t length;
        int16_t font_ascent;
        int16_t font_descent;
        int16_t overall_ascent;
        int16_t overall_descent;
        int32_t overall_width;
        int32_t overall_left;
        int32_t overall_right;
        uint32_t pad;
    } __attribute__((packed)) reply;

    memset(&reply, 0, sizeof(reply));
    reply.type = 1;
    reply.sequence_number = c->sequence;
    reply.length = 0;
    reply.font_ascent = 13;
    reply.font_descent = 3;
    reply.overall_ascent = 13;
    reply.overall_descent = 3;
    reply.overall_width = (int32_t)(num_chars * 8);
    reply.overall_left = 0;
    reply.overall_right = reply.overall_width;

    client_send_reply(c, &reply, sizeof(reply));
}

static void handle_list_fonts(client_t *c, const uint8_t *req, size_t len) {
    (void)req; (void)len;
    const char font_name[] = "fixed";
    uint8_t name_len = (uint8_t)strlen(font_name);
    size_t data_len = 1 + name_len;
    size_t pad = (4 - (data_len % 4)) % 4;
    size_t total_payload = data_len + pad;

    struct {
        uint8_t type;
        uint8_t pad1;
        uint16_t sequence_number;
        uint32_t length;
        uint16_t nfonts;
        uint8_t pad[22];
    } __attribute__((packed)) hdr;

    memset(&hdr, 0, sizeof(hdr));
    hdr.type = 1;
    hdr.sequence_number = c->sequence;
    hdr.length = (uint32_t)(total_payload / 4);
    hdr.nfonts = 1;

    uint8_t buf[128];
    memcpy(buf, &hdr, sizeof(hdr));
    buf[sizeof(hdr)] = name_len;
    memcpy(&buf[sizeof(hdr) + 1], font_name, name_len);
    if (pad) memset(&buf[sizeof(hdr) + 1 + name_len], 0, pad);

    client_send_reply(c, buf, sizeof(hdr) + total_payload);
}

static void handle_translate_coords(client_t *c, const uint8_t *req, size_t len) {
    if (len < 16) return;
    uint32_t src_wid = *(const uint32_t *)(req + 4);
    uint32_t dst_wid = *(const uint32_t *)(req + 8);
    int16_t src_x = *(const int16_t *)(req + 12);
    int16_t src_y = *(const int16_t *)(req + 14);

    window_t *src_win = window_find(src_wid);
    window_t *dst_win = window_find(dst_wid);

    int16_t root_x = src_x + (src_win ? src_win->x : 0);
    int16_t root_y = src_y + (src_win ? src_win->y : 0);
    int16_t dst_x = root_x - (dst_win ? dst_win->x : 0);
    int16_t dst_y = root_y - (dst_win ? dst_win->y : 0);

    struct {
        uint8_t type;
        uint8_t same_screen;
        uint16_t sequence_number;
        uint32_t length;
        uint32_t child;
        int16_t dst_x;
        int16_t dst_y;
        uint8_t pad[16];
    } __attribute__((packed)) reply;

    memset(&reply, 0, sizeof(reply));
    reply.type = 1;
    reply.same_screen = 1;
    reply.sequence_number = c->sequence;
    reply.length = 0;
    reply.child = 0;
    reply.dst_x = dst_x;
    reply.dst_y = dst_y;

    client_send_reply(c, &reply, sizeof(reply));
}

static void handle_set_input_focus(client_t *c, const uint8_t *req, size_t len) {
    (void)c;
    if (len < 12) return;
    uint32_t wid = *(const uint32_t *)(req + 4);
    window_t *win = window_find(wid);
    if (win) {
        window_set_focus(win);
    }
}

static void handle_get_input_focus(client_t *c, const uint8_t *req, size_t len) {
    (void)req; (void)len;
    struct {
        uint8_t type;
        uint8_t revert_to;
        uint16_t sequence_number;
        uint32_t length;
        uint32_t focus;
        uint8_t pad[20];
    } __attribute__((packed)) reply;

    memset(&reply, 0, sizeof(reply));
    reply.type = 1;
    reply.revert_to = 1; /* RevertToPointerRoot */
    reply.sequence_number = c->sequence;
    reply.length = 0;
    reply.focus = g_server.focus_window ? g_server.focus_window->id : 1;

    client_send_reply(c, &reply, sizeof(reply));
}

static void handle_grab_pointer(client_t *c, const uint8_t *req, size_t len) {
    (void)req; (void)len;
    struct {
        uint8_t type;
        uint8_t status;
        uint16_t sequence_number;
        uint32_t length;
        uint8_t pad[24];
    } __attribute__((packed)) reply;

    memset(&reply, 0, sizeof(reply));
    reply.type = 1;
    reply.status = 0; /* GrabSuccess */
    reply.sequence_number = c->sequence;
    reply.length = 0;

    client_send_reply(c, &reply, sizeof(reply));
}

static void handle_clear_area(client_t *c, const uint8_t *req, size_t len) {
    (void)c;
    if (len < 16) return;
    uint8_t exposures = req[1];
    uint32_t wid = *(const uint32_t *)(req + 4);
    int16_t x = *(const int16_t *)(req + 8);
    int16_t y = *(const int16_t *)(req + 10);
    uint16_t w = *(const uint16_t *)(req + 12);
    uint16_t h = *(const uint16_t *)(req + 14);

    window_t *win = window_find(wid);
    if (!win) return;

    if (w == 0) w = (win->width > x) ? (win->width - x) : 0;
    if (h == 0) h = (win->height > y) ? (win->height - y) : 0;

    if (win->backing_pixmap && win->backing_pixmap->data) {
        draw_fill_rect(win->backing_pixmap->data, win->backing_pixmap->pitch,
                       win->backing_pixmap->width, win->backing_pixmap->height,
                       x, y, w, h, win->background_pixel, 3);
    }

    if (exposures) {
        x11_expose_event_t exp;
        memset(&exp, 0, sizeof(exp));
        exp.type = X_Expose;
        exp.window = win->id;
        exp.x = x;
        exp.y = y;
        exp.width = w;
        exp.height = h;
        exp.count = 0;
        window_send_event(win, &exp, ExposureMask);
    }
}

void dispatch_request(client_t *c, const uint8_t *req, size_t len) {
    if (!c || !req || len < 4) return;
    uint8_t opcode = req[0];
    if (opcode != X_PolyFillRectangle && opcode != X_PolyText8 && opcode != X_PutImage && opcode != X_AllocColor) {
        printf("[SzpontX11] FD %d Opcode %d (len %zu)\n", c->fd, opcode, len);
        fflush(stdout);
    }

    switch (opcode) {
    case X_CreateWindow:            handle_create_window(c, req, len); break;
    case X_ChangeWindowAttributes:  handle_change_window_attributes(c, req, len); break;
    case X_GetWindowAttributes:     handle_get_window_attributes(c, req, len); break;
    case X_DestroyWindow:           window_destroy(window_find(*(const uint32_t *)(req + 4))); break;
    case X_MapWindow:               window_map(window_find(*(const uint32_t *)(req + 4))); break;
    case X_MapSubwindows: {
        window_t *parent = window_find(*(const uint32_t *)(req + 4));
        if (parent) {
            for (window_t *ch = parent->first_child; ch != NULL; ch = ch->next_sibling) {
                window_map(ch);
            }
        }
        break;
    }
    case X_UnmapWindow:             window_unmap(window_find(*(const uint32_t *)(req + 4))); break;
    case X_UnmapSubwindows: {
        window_t *parent = window_find(*(const uint32_t *)(req + 4));
        if (parent) {
            for (window_t *ch = parent->first_child; ch != NULL; ch = ch->next_sibling) {
                window_unmap(ch);
            }
        }
        break;
    }
    case X_ConfigureWindow:         handle_configure_window(c, req, len); break;
    case X_CopyArea:                handle_copy_area(c, req, len); break;
    case X_ClearArea:               handle_clear_area(c, req, len); break;
    case X_GetGeometry:             handle_get_geometry(c, req, len); break;
    case X_QueryTree:               handle_query_tree(c, req, len); break;
    case X_InternAtom:              handle_intern_atom(c, req, len); break;
    case X_GetAtomName:             handle_get_atom_name(c, req, len); break;
    case X_ChangeProperty:          handle_change_property(c, req, len); break;
    case X_GetProperty:             handle_get_property(c, req, len); break;
    case X_CreatePixmap:            handle_create_pixmap(c, req, len); break;
    case X_FreePixmap:              pixmap_destroy(pixmap_find(*(const uint32_t *)(req + 4))); break;
    case X_CreateGC:                handle_create_gc(c, req, len); break;
    case X_ChangeGC:                handle_change_gc(c, req, len); break;
    case X_FreeGC:                  gc_destroy(gc_find(*(const uint32_t *)(req + 4))); break;
    case X_PolyFillRectangle:       handle_poly_fill_rectangle(c, req, len); break;
    case X_PolyRectangle:           handle_poly_rectangle(c, req, len); break;
    case X_PolyLine:                handle_poly_line(c, req, len); break;
    case X_PolyArc:                 handle_poly_arc(c, req, len, false); break;
    case X_PolyFillArc:             handle_poly_arc(c, req, len, true); break;
    case X_PutImage:                handle_put_image(c, req, len); break;
    case X_ImageText8:              handle_image_text8(c, req, len); break;
    case X_PolyText8:               handle_poly_text8(c, req, len); break;
    case X_AllocColor:              handle_alloc_color(c, req, len); break;
    case X_AllocNamedColor:         handle_alloc_named_color(c, req, len); break;
    case X_QueryColors:             handle_query_colors(c, req, len); break;
    case X_LookupColor:             handle_lookup_color(c, req, len); break;
    case X_GetKeyboardMapping:      handle_get_keyboard_mapping(c, req, len); break;
    case X_GetModifierMapping:      handle_get_modifier_mapping(c); break;
    case X_QueryPointer:            handle_query_pointer(c, req, len); break;
    case X_GrabPointer:             handle_grab_pointer(c, req, len); break;
    case X_UngrabPointer:           break;
    case X_GrabKeyboard:            handle_grab_pointer(c, req, len); break;
    case X_UngrabKeyboard:          break;
    case X_TranslateCoords:         handle_translate_coords(c, req, len); break;
    case X_SetInputFocus:           handle_set_input_focus(c, req, len); break;
    case X_GetInputFocus:           handle_get_input_focus(c, req, len); break;
    case X_OpenFont:                handle_open_font(c, req, len); break;
    case X_CloseFont:               handle_close_font(c, req, len); break;
    case X_QueryFont:               handle_query_font(c, req, len); break;
    case X_QueryTextExtents:        handle_query_text_extents(c, req, len); break;
    case X_ListFonts:               handle_list_fonts(c, req, len); break;
    case X_ListFontsWithInfo:       handle_list_fonts(c, req, len); break;
    case X_NoOperation:             break;
    case X_QueryExtension: {
        uint16_t nbytes = (len >= 8) ? *(const uint16_t *)(req + 4) : 0;
        const char *name = (const char *)(req + 8);
        struct {
            x11_reply_header_t hdr;
            uint8_t present;
            uint8_t major_opcode;
            uint8_t first_event;
            uint8_t first_error;
            uint8_t pad[20];
        } ext_reply;
        memset(&ext_reply, 0, sizeof(ext_reply));
        ext_reply.hdr.response_type = 1;

        if (nbytes == 7 && strncmp(name, "MIT-SHM", 7) == 0) {
            ext_reply.present = 1;
            ext_reply.major_opcode = X_Shm_Opcode;
            ext_reply.first_event = ShmCompletionEvent;
            ext_reply.first_error = BadShmSeg;
            printf("[SzpontX11] QueryExtension: MIT-SHM enabled (Opcode %d, Event %d, Error %d)\n",
                   X_Shm_Opcode, ShmCompletionEvent, BadShmSeg);
        }

        client_send_reply(c, &ext_reply, sizeof(ext_reply));
        break;
    }
    case X_Shm_Opcode: {
        uint8_t minor = req[1];
        switch (minor) {
        case X_ShmQueryVersion: handle_shm_query_version(c); break;
        case X_ShmAttach:       handle_shm_attach(c, req, len); break;
        case X_ShmDetach:       handle_shm_detach(c, req, len); break;
        case X_ShmPutImage:     handle_shm_put_image(c, req, len); break;
        case X_ShmCreatePixmap: handle_shm_create_pixmap(c, req, len); break;
        default: break;
        }
        break;
    }
    default:
        break;
    }
}
