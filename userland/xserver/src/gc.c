/*
 * SzpontOS - SzpontX11 Native X11 Server
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * Graphics Contexts (GC) and Pixmaps Management
 */

#include "xserver.h"
#include <stdlib.h>
#include <string.h>

gc_t *gc_create(client_t *c, uint32_t id) {
    for (int i = 0; i < MAX_GCS; i++) {
        if (!g_server.gcs[i].is_active) {
            gc_t *gc = &g_server.gcs[i];
            memset(gc, 0, sizeof(gc_t));
            gc->id = id;
            gc->owner = c;
            gc->function = GXcopy;
            gc->foreground = 0xFFFFFFFF; /* Default white */
            gc->background = 0x00000000; /* Default black */
            gc->line_width = 1;
            gc->plane_mask = ~0U;
            gc->is_active = true;
            return gc;
        }
    }
    return NULL;
}

gc_t *gc_find(uint32_t id) {
    for (int i = 0; i < MAX_GCS; i++) {
        if (g_server.gcs[i].is_active && g_server.gcs[i].id == id) {
            return &g_server.gcs[i];
        }
    }
    return NULL;
}

void gc_destroy(gc_t *gc) {
    if (gc) {
        gc->is_active = false;
        gc->id = 0;
    }
}

pixmap_t *pixmap_create(client_t *c, uint32_t id, int w, int h, int depth) {
    for (int i = 0; i < MAX_PIXMAPS; i++) {
        if (!g_server.pixmaps[i].is_active) {
            pixmap_t *pm = &g_server.pixmaps[i];
            memset(pm, 0, sizeof(pixmap_t));
            pm->id = id;
            pm->owner = c;
            pm->width = w;
            pm->height = h;
            pm->depth = depth;
            pm->pitch = w * 4;
            pm->data = (uint32_t *)calloc(w * h, sizeof(uint32_t));
            if (!pm->data)
                return NULL;
            pm->is_active = true;
            return pm;
        }
    }
    return NULL;
}

pixmap_t *pixmap_find(uint32_t id) {
    for (int i = 0; i < MAX_PIXMAPS; i++) {
        if (g_server.pixmaps[i].is_active && g_server.pixmaps[i].id == id) {
            return &g_server.pixmaps[i];
        }
    }
    return NULL;
}

void pixmap_destroy(pixmap_t *pm) {
    if (pm) {
        if (pm->data) {
            free(pm->data);
            pm->data = NULL;
        }
        pm->is_active = false;
        pm->id = 0;
    }
}
