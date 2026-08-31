/*
 * SzpontOS - SzpontX11 Native X11 Server
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * Client Connection Management, Handshake & Stream Buffering
 */

#include "xserver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

static uint32_t g_next_xid_base = 0x00200000;

void client_init_system(void) {
    memset(g_server.clients, 0, sizeof(g_server.clients));
}

client_t *client_accept(int listen_fd) {
    struct sockaddr addr;
    socklen_t addrlen = sizeof(addr);
    int client_fd = accept(listen_fd, &addr, &addrlen);
    if (client_fd < 0) return NULL;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!g_server.clients[i].active) {
            client_t *c = &g_server.clients[i];
            memset(c, 0, sizeof(client_t));
            c->fd = client_fd;
            c->active = true;
            c->authenticated = false;
            c->sequence = 0;
            c->xid_base = g_next_xid_base;
            c->xid_mask = 0x001FFFFF;
            g_next_xid_base += 0x00200000;
            printf("[SzpontX11] New X11 client connected! (FD %d, Slot %d)\n", client_fd, i);
            fflush(stdout);
            return c;
        }
    }

    fprintf(stderr, "[SzpontX11] Maximum clients reached, rejecting FD %d\n", client_fd);
    fflush(stderr);
    close(client_fd);
    return NULL;
}

void client_close(client_t *c) {
    if (!c || !c->active) return;

    /* Destroy all windows belonging to this client */
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (g_server.windows[i].is_active && g_server.windows[i].owner == c) {
            window_destroy(&g_server.windows[i]);
        }
    }

    /* Destroy all GCs belonging to this client */
    for (int i = 0; i < MAX_GCS; i++) {
        if (g_server.gcs[i].is_active && g_server.gcs[i].owner == c) {
            gc_destroy(&g_server.gcs[i]);
        }
    }

    /* Destroy all Pixmaps belonging to this client */
    for (int i = 0; i < MAX_PIXMAPS; i++) {
        if (g_server.pixmaps[i].is_active && g_server.pixmaps[i].owner == c) {
            pixmap_destroy(&g_server.pixmaps[i]);
        }
    }

    if (c->fd >= 0) {
        close(c->fd);
        c->fd = -1;
    }

    c->active = false;
    c->authenticated = false;
    c->in_len = 0;
    c->out_len = 0;
    c->event_count = 0;
}

void client_send_reply(client_t *c, const void *data, size_t len) {
    if (!c || !c->active || !data || len == 0) return;

    x11_reply_header_t *hdr = (x11_reply_header_t *)data;
    hdr->sequence_number = c->sequence;

    /* Pad to 32 bytes minimum if it's a standard reply */
    uint8_t padded_buf[256];
    const void *send_ptr = data;
    size_t send_len = len;

    if (len < 32) {
        memset(padded_buf, 0, 32);
        memcpy(padded_buf, data, len);
        send_ptr = padded_buf;
        send_len = 32;
    }

    ssize_t sent = send(c->fd, send_ptr, send_len, 0);
    (void)sent;
}

void client_send_error(client_t *c, uint8_t error_code, uint8_t major_opcode, uint16_t minor_opcode, uint32_t bad_value) {
    if (!c || !c->active) return;

    x11_error_event_t err;
    memset(&err, 0, sizeof(err));
    err.response_type = 0;
    err.error_code = error_code;
    err.sequence_number = c->sequence;
    err.bad_value = bad_value;
    err.minor_opcode = minor_opcode;
    err.major_opcode = major_opcode;

    send(c->fd, &err, 32, 0);
}

static void send_connection_setup_reply(client_t *c) {
    const char vendor[] = "Szpont Industries";
    uint16_t vendor_len = (uint16_t)strlen(vendor);
    uint16_t vendor_pad = (4 - (vendor_len % 4)) % 4;

    /* Setup components */
    x11_conn_setup_prefix_t prefix;
    memset(&prefix, 0, sizeof(prefix));
    prefix.status = 1; /* Success */
    prefix.major_version = 11;
    prefix.minor_version = 0;

    x11_conn_setup_t setup;
    memset(&setup, 0, sizeof(setup));
    setup.release_number = 1;
    setup.resource_id_base = c->xid_base;
    setup.resource_id_mask = c->xid_mask;
    setup.motion_buffer_size = 256;
    setup.vendor_len = vendor_len;
    setup.max_request_size = 65535;
    setup.num_screens = 1;
    setup.num_formats = 2;
    setup.image_byte_order = 0; /* LSBFirst */
    setup.bitmap_bit_order = 0; /* LSBFirst */
    setup.bitmap_format_scanline_unit = 32;
    setup.bitmap_format_scanline_pad = 32;
    setup.min_keycode = 8;
    setup.max_keycode = 255;

    /* Pixmap Formats (32bpp and 1bpp) */
    x11_pixmap_format_t formats[2];
    memset(formats, 0, sizeof(formats));
    formats[0].depth = 1;
    formats[0].bits_per_pixel = 1;
    formats[0].scanline_pad = 32;

    formats[1].depth = 24;
    formats[1].bits_per_pixel = 32;
    formats[1].scanline_pad = 32;

    /* Screen Description */
    x11_screen_t scr;
    memset(&scr, 0, sizeof(scr));
    scr.window_id = 1; /* Root Window */
    scr.colormap_id = 1;
    scr.white_pixel = 0xFFFFFFFF;
    scr.black_pixel = 0x00000000;
    scr.current_input_masks = 0;
    scr.width_in_pixels = (uint16_t)g_server.width;
    scr.height_in_pixels = (uint16_t)g_server.height;
    scr.width_in_millimeters = (uint16_t)(g_server.width * 25.4 / 96.0);
    scr.height_in_millimeters = (uint16_t)(g_server.height * 25.4 / 96.0);
    scr.min_installed_maps = 1;
    scr.max_installed_maps = 1;
    scr.root_visual_id = 1;
    scr.backing_stores = 2; /* Always */
    scr.save_unders = 1;
    scr.root_depth = 24;
    scr.num_depths = 1;

    /* Depth and Visual */
    x11_depth_t depth;
    memset(&depth, 0, sizeof(depth));
    depth.depth = 24;
    depth.num_visuals = 1;

    x11_visual_type_t visual;
    memset(&visual, 0, sizeof(visual));
    visual.visual_id = 1;
    visual.class_val = TrueColor;
    visual.bits_per_rgb = 8;
    visual.colormap_entries = 256;
    visual.red_mask   = 0x00FF0000;
    visual.green_mask = 0x0000FF00;
    visual.blue_mask  = 0x000000FF;

    size_t extra_len = sizeof(setup) + vendor_len + vendor_pad +
                       sizeof(formats) + sizeof(scr) + sizeof(depth) + sizeof(visual);

    prefix.length = (uint16_t)(extra_len / 4);

    /* Assemble and send entire handshake reply atomically */
    uint8_t reply_buf[1024];
    size_t offset = 0;

    memcpy(&reply_buf[offset], &prefix, sizeof(prefix));
    offset += sizeof(prefix);

    memcpy(&reply_buf[offset], &setup, sizeof(setup));
    offset += sizeof(setup);

    memcpy(&reply_buf[offset], vendor, vendor_len);
    offset += vendor_len;

    if (vendor_pad > 0) {
        memset(&reply_buf[offset], 0, vendor_pad);
        offset += vendor_pad;
    }

    memcpy(&reply_buf[offset], formats, sizeof(formats));
    offset += sizeof(formats);

    memcpy(&reply_buf[offset], &scr, sizeof(scr));
    offset += sizeof(scr);

    memcpy(&reply_buf[offset], &depth, sizeof(depth));
    offset += sizeof(depth);

    memcpy(&reply_buf[offset], &visual, sizeof(visual));
    offset += sizeof(visual);

    send(c->fd, reply_buf, offset, 0);
}

void client_process_data(client_t *c) {
    if (!c || !c->active) return;

    /* Read as much data as available into in_buf */
    while (c->in_len < CLIENT_BUF_SIZE) {
        ssize_t n = recv(c->fd, &c->in_buf[c->in_len], CLIENT_BUF_SIZE - c->in_len, MSG_DONTWAIT);
        if (n <= 0) {
            if (n == 0) {
                client_close(c);
                return;
            }
            break; /* No more data currently available (-EAGAIN) */
        }
        c->in_len += (size_t)n;
    }

    if (c->in_len == 0) return;

    /* 1. Handle X11 Connection Handshake */
    if (!c->authenticated) {
        if (c->in_len < sizeof(x11_conn_client_prefix_t)) {
            return;
        }

        x11_conn_client_prefix_t *pfx = (x11_conn_client_prefix_t *)c->in_buf;
        size_t total_handshake_len = sizeof(x11_conn_client_prefix_t) +
                                     pfx->auth_proto_len + ((4 - (pfx->auth_proto_len % 4)) % 4) +
                                     pfx->auth_data_len + ((4 - (pfx->auth_data_len % 4)) % 4);

        if (c->in_len < total_handshake_len) {
            return;
        }

        send_connection_setup_reply(c);
        c->authenticated = true;
        printf("[SzpontX11] Sent connection setup reply to client FD %d (Vendor: Szpont Industries)\n", c->fd);
        fflush(stdout);

        /* Advance buffer */
        c->in_len -= total_handshake_len;
        if (c->in_len > 0) {
            memmove(c->in_buf, &c->in_buf[total_handshake_len], c->in_len);
        }
    }

    int reqs_processed = 0;
    /* 2. Process Requests */
    while (c->authenticated && c->in_len >= sizeof(x11_req_header_t)) {
        x11_req_header_t *hdr = (x11_req_header_t *)c->in_buf;
        size_t req_len = (size_t)hdr->length * 4;
        if (req_len < sizeof(x11_req_header_t)) {
            req_len = sizeof(x11_req_header_t);
        }

        if (c->in_len < req_len) {
            break; /* Incomplete packet, wait for more data */
        }

        c->sequence++;
        dispatch_request(c, c->in_buf, req_len);
        reqs_processed++;

        c->in_len -= req_len;
        if (c->in_len > 0) {
            memmove(c->in_buf, &c->in_buf[req_len], c->in_len);
        }
    }

    if (reqs_processed > 0) {
        g_server.needs_redraw = true;
    }

    /* 3. Flush queued events to client socket */
    client_flush_events(c);
}

void client_flush_events(client_t *c) {
    if (!c || !c->active || c->fd < 0) return;

    while (c->event_count > 0) {
        send(c->fd, c->event_queue[c->event_head], 32, 0);
        c->event_head = (c->event_head + 1) % EVENT_QUEUE_SIZE;
        c->event_count--;
    }
}
