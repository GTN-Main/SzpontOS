/*
 * SzpontOS - SzpontX11 Native X11 Server
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * Core Server Architecture & Internal Structures
 */

#ifndef SZPONT_XSERVER_H
#define SZPONT_XSERVER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>
#include "xproto_defs.h"

#define MAX_CLIENTS         32
#define MAX_WINDOWS         256
#define MAX_GCS             128
#define MAX_PIXMAPS         128
#define MAX_ATOMS           256
#define MAX_CLIPS           32

#define CLIENT_BUF_SIZE     (32 * 1024)
#define EVENT_QUEUE_SIZE    128

/* Forward declarations */
typedef struct window window_t;
typedef struct client client_t;
typedef struct gc gc_t;
typedef struct pixmap pixmap_t;
typedef struct property property_t;

struct property {
    uint32_t atom;
    uint32_t type;
    uint8_t  format; /* 8, 16, 32 */
    void     *data;
    size_t   data_len;
    property_t *next;
};

struct pixmap {
    uint32_t id;
    client_t *owner;
    int      width;
    int      height;
    int      depth;
    int      pitch;
    uint32_t *data;
    bool     is_active;
};

struct gc {
    uint32_t id;
    client_t *owner;
    uint8_t  function; /* GXcopy, etc. */
    uint32_t foreground;
    uint32_t background;
    uint16_t line_width;
    uint32_t plane_mask;
    uint8_t  fill_style;
    int16_t  clip_x_origin;
    int16_t  clip_y_origin;
    int      num_clip_rects;
    struct {
        int16_t x, y;
        uint16_t width, height;
    } clip_rects[MAX_CLIPS];
    bool     is_active;
};

struct window {
    uint32_t id;
    client_t *owner;
    window_t *parent;
    window_t *first_child;
    window_t *next_sibling;

    int16_t  x;
    int16_t  y;
    uint16_t width;
    uint16_t height;
    uint16_t border_width;
    uint8_t  depth;
    uint32_t visual_id;

    bool     mapped;
    bool     override_redirect;
    uint32_t background_pixel;
    uint32_t border_pixel;
    uint32_t event_mask;
    client_t *event_clients[MAX_CLIENTS];
    uint32_t client_event_masks[MAX_CLIENTS];

    pixmap_t *backing_pixmap;
    property_t *properties;
    bool     is_active;

    /* Window Decorator & Compositor State */
    char     title[64];
    bool     is_decorated;
    bool     is_maximized;
    int16_t  restore_x;
    int16_t  restore_y;
    uint16_t restore_w;
    uint16_t restore_h;
};

#define TITLEBAR_HEIGHT     28
#define WINDOW_BORDER_W     2
#define BUTTON_SIZE         12

typedef struct {
    uint32_t id;
    char     *name;
} atom_entry_t;

#define MAX_SHMSEGS         64

typedef struct shmseg {
    uint32_t seg_id;       /* X11 SHM Seg ID */
    int      shmid;        /* Kernel IPC SHMID */
    void     *mapped_addr; /* Address in Xserver process */
    size_t   size;
    bool     read_only;
    bool     active;
} shmseg_t;

struct client {
    int      fd;
    bool     active;
    bool     authenticated;
    uint16_t sequence;

    uint32_t xid_base;
    uint32_t xid_mask;

    uint8_t  *in_buf;
    size_t   in_len;
    size_t   in_cap;

    uint32_t event_queue[EVENT_QUEUE_SIZE][8]; /* 32 bytes per X11 wire event */
    int      event_head;
    int      event_tail;
    int      event_count;

    shmseg_t shm_segments[MAX_SHMSEGS];
};

typedef struct {
    /* Screen and Framebuffer (Hardware Double Buffering) */
    int      drm_fd;
    uint32_t crtc_id;
    uint32_t conn_id;

    /* Front Buffer (Current CRTC Scanout) */
    uint32_t front_fb_id;
    uint32_t front_dumb_handle;
    uint32_t *front_fb_mapped;

    /* Back Buffer (Active Render Target) */
    uint32_t back_fb_id;
    uint32_t back_dumb_handle;
    uint32_t *back_fb_mapped;

    /* Compatibility pointers */
    uint32_t fb_id;
    uint32_t dumb_handle;
    uint32_t *fb_mapped;
    uint32_t *shadow_fb;

    int      width;
    int      height;
    int      pitch;
    int      bpp;
    int      screen_dpi;

    /* Socket listeners */
    int      unix_listen_fd;
    int      tcp_listen_fd;

    /* Input devices */
    int      mouse_fd;
    int      kbd_fd;
    int      mice_fd;
    int      psaux_fd;
    int      devmouse_fd;
    int      mouse_x;
    int      mouse_y;
    uint32_t mouse_buttons;
    bool     shift_pressed;
    bool     caps_locked;
    bool     ctrl_pressed;
    bool     alt_pressed;

    /* Resources */
    window_t root_window;
    window_t windows[MAX_WINDOWS];
    gc_t     gcs[MAX_GCS];
    pixmap_t pixmaps[MAX_PIXMAPS];
    atom_entry_t atoms[MAX_ATOMS];
    int      num_atoms;

    client_t clients[MAX_CLIENTS];
    window_t *focus_window;
    window_t *pointer_window;
    window_t *grab_window;
    window_t *dragging_window;
    int      drag_offset_x;
    int      drag_offset_y;
    uint8_t  active_cursor_type;
    bool     needs_redraw;
    bool     running;
} server_t;

enum cursor_type {
    CURSOR_ARROW = 0,
    CURSOR_IBEAM,
    CURSOR_HAND,
    CURSOR_RESIZE_NWSE,
    CURSOR_RESIZE_NESW,
    CURSOR_RESIZE_WE,
    CURSOR_RESIZE_NS,
    CURSOR_MOVE,
    CURSOR_WAIT,
    CURSOR_CROSSHAIR,
    MAX_CURSOR_TYPES
};

extern server_t g_server;

/* Function prototypes */

/* DRM / Display */
bool drm_init_display(void);
void drm_cleanup_display(void);
void drm_flush_rect(int x, int y, int w, int h);
void drm_flush_screen(void);
void drm_swap_buffers(void);

/* SHM Segment Management */
shmseg_t *shm_find_seg(client_t *c, uint32_t seg_id);
void shm_cleanup_client(client_t *c);

/* Input */
bool input_init(void);
void input_cleanup(void);
void input_process_events(void);

/* Windows */
void window_init_system(void);
window_t *window_create(client_t *c, uint32_t id, window_t *parent, int16_t x, int16_t y,
                        uint16_t w, uint16_t h, uint16_t bw, uint8_t depth, uint32_t visual);
window_t *window_find(uint32_t id);
void window_destroy(window_t *win);
void window_map(window_t *win);
void window_unmap(window_t *win);
void window_configure(window_t *win, int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t bw);
void window_raise(window_t *win);
void window_set_focus(window_t *win);
window_t *window_find_at_pos(int x, int y);
void window_get_absolute_coords(window_t *win, int *abs_x, int *abs_y);
void window_send_event(window_t *win, void *event_wire_32bytes, uint32_t event_mask);

/* GC & Pixmaps */
gc_t *gc_create(client_t *c, uint32_t id);
gc_t *gc_find(uint32_t id);
void gc_destroy(gc_t *gc);
pixmap_t *pixmap_create(client_t *c, uint32_t id, int w, int h, int depth);
pixmap_t *pixmap_find(uint32_t id);
void pixmap_destroy(pixmap_t *pm);

/* Atoms & Properties */
void atom_init_system(void);
uint32_t atom_intern(const char *name, bool only_if_exists);
const char *atom_get_name(uint32_t atom_id);
void property_set(window_t *win, uint32_t atom, uint32_t type, uint8_t format, const void *data, size_t len);
property_t *property_get(window_t *win, uint32_t atom);
void property_delete(window_t *win, uint32_t atom);

/* 2D Drawing & Compositing */
void draw_init_system(void);
void draw_fill_rect(uint32_t *dst, int pitch, int dst_w, int dst_h, int x, int y, int w, int h, uint32_t color, uint8_t rop);
void draw_rect(uint32_t *dst, int pitch, int dst_w, int dst_h, int x, int y, int w, int h, uint32_t color, int line_width);
void draw_line(uint32_t *dst, int pitch, int dst_w, int dst_h, int x1, int y1, int x2, int y2, uint32_t color, int line_width);
void draw_arc(uint32_t *dst, int pitch, int dst_w, int dst_h, int x, int y, int w, int h, int angle1, int angle2, uint32_t color, bool fill);
void draw_glyph(uint32_t *dst, int pitch, int dst_w, int dst_h, int x, int y, uint8_t char_code, uint32_t fg, uint32_t bg, bool transparent);
void draw_text(uint32_t *dst, int pitch, int dst_w, int dst_h, int x, int y, const char *str, size_t len, uint32_t fg, uint32_t bg, bool transparent);
void draw_blit(uint32_t *dst, int dst_pitch, int dst_w, int dst_h, int dst_x, int dst_y,
               const uint32_t *src, int src_pitch, int src_w, int src_h, int src_x, int src_y, int w, int h);
void draw_composite_scene(void);
void draw_cursor(uint32_t *dst, int pitch, int dst_w, int dst_h, int cx, int cy);
void draw_update_cursor(void);

/* Dispatch & Client Handling */
void client_init_system(void);
client_t *client_accept(int listen_fd);
void client_process_data(client_t *c);
void client_flush_events(client_t *c);
void client_send_reply(client_t *c, const void *data, size_t len);
void client_send_error(client_t *c, uint8_t error_code, uint8_t major_opcode, uint16_t minor_opcode, uint32_t bad_value);
void client_close(client_t *c);
void dispatch_request(client_t *c, const uint8_t *req, size_t len);

#endif /* SZPONT_XSERVER_H */
