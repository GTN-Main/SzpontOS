/*
 * SzpontOS - SzpontX11 Native X11 Server
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * Atom Table & Window Property Management
 */

#include "xserver.h"
#include <stdlib.h>
#include <string.h>

static const struct {
    uint32_t id;
    const char *name;
} g_predefined_atoms[] = {
    { XA_PRIMARY,           "PRIMARY" },
    { XA_SECONDARY,         "SECONDARY" },
    { XA_ARC,               "ARC" },
    { XA_ATOM,              "ATOM" },
    { XA_BITMAP,            "BITMAP" },
    { XA_CARDINAL,          "CARDINAL" },
    { XA_COLORMAP,          "COLORMAP" },
    { XA_CURSOR,            "CURSOR" },
    { XA_CUT_BUFFER0,       "CUT_BUFFER0" },
    { XA_DRAWABLE,          "DRAWABLE" },
    { XA_FONT,              "FONT" },
    { XA_INTEGER,           "INTEGER" },
    { XA_PIXMAP,            "PIXMAP" },
    { XA_POINT,             "POINT" },
    { XA_RECTANGLE,         "RECTANGLE" },
    { XA_RESOURCE_MANAGER,  "RESOURCE_MANAGER" },
    { XA_RGB_COLOR_MAP,     "RGB_COLOR_MAP" },
    { XA_STRING,            "STRING" },
    { XA_VISUALID,          "VISUALID" },
    { XA_WINDOW,            "WINDOW" },
    { XA_WM_COMMAND,        "WM_COMMAND" },
    { XA_WM_HINTS,          "WM_HINTS" },
    { XA_WM_CLIENT_MACHINE, "WM_CLIENT_MACHINE" },
    { XA_WM_ICON_NAME,      "WM_ICON_NAME" },
    { XA_WM_ICON_SIZE,      "WM_ICON_SIZE" },
    { XA_WM_NAME,           "WM_NAME" },
    { XA_WM_NORMAL_HINTS,   "WM_NORMAL_HINTS" },
    { XA_WM_SIZE_HINTS,     "WM_SIZE_HINTS" },
    { XA_WM_ZOOM_HINTS,     "WM_ZOOM_HINTS" },
    { XA_WM_CLASS,          "WM_CLASS" },
    { XA_WM_TRANSIENT_FOR,  "WM_TRANSIENT_FOR" },
    { 0, NULL }
};

void atom_init_system(void) {
    g_server.num_atoms = 0;
    for (int i = 0; g_predefined_atoms[i].name != NULL; i++) {
        uint32_t id = g_predefined_atoms[i].id;
        if (id < MAX_ATOMS) {
            g_server.atoms[id].id = id;
            g_server.atoms[id].name = strdup(g_predefined_atoms[i].name);
            if (id >= (uint32_t)g_server.num_atoms) {
                g_server.num_atoms = id + 1;
            }
        }
    }
}

uint32_t atom_intern(const char *name, bool only_if_exists) {
    if (!name || !*name)
        return 0;

    for (int i = 1; i < g_server.num_atoms; i++) {
        if (g_server.atoms[i].name && strcmp(g_server.atoms[i].name, name) == 0) {
            return g_server.atoms[i].id;
        }
    }

    if (only_if_exists)
        return 0;

    if (g_server.num_atoms >= MAX_ATOMS)
        return 0;

    uint32_t new_id = g_server.num_atoms++;
    g_server.atoms[new_id].id = new_id;
    g_server.atoms[new_id].name = strdup(name);
    return new_id;
}

const char *atom_get_name(uint32_t atom_id) {
    if (atom_id == 0 || atom_id >= (uint32_t)g_server.num_atoms)
        return NULL;
    return g_server.atoms[atom_id].name;
}

void property_set(window_t *win, uint32_t atom, uint32_t type, uint8_t format, const void *data, size_t len) {
    if (!win || atom == 0)
        return;

    property_t *prop = property_get(win, atom);
    if (!prop) {
        prop = (property_t *)calloc(1, sizeof(property_t));
        if (!prop) return;
        prop->atom = atom;
        prop->next = win->properties;
        win->properties = prop;
    } else if (prop->data) {
        free(prop->data);
        prop->data = NULL;
    }

    prop->type = type;
    prop->format = format;
    prop->data_len = len;
    if (len > 0 && data) {
        prop->data = malloc(len);
        if (prop->data) {
            memcpy(prop->data, data, len);
        }
    }
}

property_t *property_get(window_t *win, uint32_t atom) {
    if (!win || atom == 0)
        return NULL;
    for (property_t *p = win->properties; p != NULL; p = p->next) {
        if (p->atom == atom)
            return p;
    }
    return NULL;
}

void property_delete(window_t *win, uint32_t atom) {
    if (!win || atom == 0)
        return;

    property_t **cur = &win->properties;
    while (*cur) {
        if ((*cur)->atom == atom) {
            property_t *to_free = *cur;
            *cur = to_free->next;
            if (to_free->data) free(to_free->data);
            free(to_free);
            return;
        }
        cur = &(*cur)->next;
    }
}
