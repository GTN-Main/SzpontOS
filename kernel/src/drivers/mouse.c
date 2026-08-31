/*
 * SzpontOS - Universal Mouse Subsystem Implementation
 * Provides event queueing and character device endpoints for mouse devices.
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/mouse.h>
#include <drivers/evdev.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/kprint.h>

#define MOUSE_EVENT_QUEUE_SIZE 256

static mouse_event_t g_mouse_queue[MOUSE_EVENT_QUEUE_SIZE];
static size_t g_mouse_head = 0;
static size_t g_mouse_tail = 0;
static spinlock_t g_mouse_lock = SPINLOCK_INIT;
static bool g_mouse_subsystem_ready = false;

void mouse_init(void) {
    spinlock_init(&g_mouse_lock);
    g_mouse_head = 0;
    g_mouse_tail = 0;
    g_mouse_subsystem_ready = true;
    klog_info("Mouse: Unified mouse subsystem initialized");
}

void mouse_push_event(const mouse_event_t *ev) {
    if (!ev || !g_mouse_subsystem_ready)
        return;

    spinlock_acquire(&g_mouse_lock);
    size_t next = (g_mouse_tail + 1) % MOUSE_EVENT_QUEUE_SIZE;
    if (next != g_mouse_head) {
        g_mouse_queue[g_mouse_tail] = *ev;
        g_mouse_tail = next;
    }
    spinlock_release(&g_mouse_lock);

    /* Bridge unified mouse event to Linux/X11 evdev and raw /dev/input/mice */
    evdev_push_mouse_packet(ev->dx, ev->dy, ev->dz, ev->buttons);
}

bool mouse_has_event(void) {
    if (!g_mouse_subsystem_ready)
        return false;
    spinlock_acquire(&g_mouse_lock);
    bool has = (g_mouse_head != g_mouse_tail);
    spinlock_release(&g_mouse_lock);
    return has;
}

bool mouse_get_event(mouse_event_t *ev) {
    if (!ev || !g_mouse_subsystem_ready)
        return false;

    spinlock_acquire(&g_mouse_lock);
    if (g_mouse_head == g_mouse_tail) {
        spinlock_release(&g_mouse_lock);
        return false;
    }
    *ev = g_mouse_queue[g_mouse_head];
    g_mouse_head = (g_mouse_head + 1) % MOUSE_EVENT_QUEUE_SIZE;
    spinlock_release(&g_mouse_lock);
    return true;
}

ssize_t mouse_devfs_read(void *buf, size_t count) {
    if (!buf || count < sizeof(mouse_event_t))
        return 0;

    mouse_event_t ev;
    if (mouse_get_event(&ev)) {
        memcpy(buf, &ev, sizeof(mouse_event_t));
        return sizeof(mouse_event_t);
    }
    return 0;
}
