/*
 * SzpontOS - Evdev & Input Subsystem Driver
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/evdev.h>
#include <drivers/rtc.h>
#include <fs/devfs.h>
#include <mm/heap.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/kprint.h>

#define EVDEV_QUEUE_SIZE 256

typedef struct {
    struct input_event events[EVDEV_QUEUE_SIZE];
    size_t head;
    size_t tail;
    size_t count;
    spinlock_t lock;
} evdev_queue_t;

static evdev_queue_t g_mouse_queue;
static evdev_queue_t g_kbd_queue;

/* Mice raw buffer (3-byte standard PS/2 packets) */
static uint8_t g_mice_buf[EVDEV_QUEUE_SIZE * 3];
static size_t g_mice_head = 0;
static size_t g_mice_tail = 0;
static size_t g_mice_count = 0;
static spinlock_t g_mice_lock = SPINLOCK_INIT;

static uint8_t g_last_mouse_buttons = 0;

static struct timeval evdev_get_timestamp(void) {
    struct timeval tv;
    struct timespec_kernel ts;
    rtc_get_monotonic(&ts);
    tv.tv_sec = ts.tv_sec;
    tv.tv_usec = ts.tv_nsec / 1000;
    return tv;
}

static void evdev_enqueue(evdev_queue_t *q, uint16_t type, uint16_t code, int32_t value) {
    spinlock_acquire(&q->lock);
    if (q->count >= EVDEV_QUEUE_SIZE) {
        /* Drop oldest event */
        q->head = (q->head + 1) % EVDEV_QUEUE_SIZE;
        q->count--;
    }

    struct input_event *ev = &q->events[q->tail];
    ev->time = evdev_get_timestamp();
    ev->type = type;
    ev->code = code;
    ev->value = value;

    q->tail = (q->tail + 1) % EVDEV_QUEUE_SIZE;
    q->count++;
    spinlock_release(&q->lock);
}

static ssize_t devfs_evdev_mice_read(vfs_node_t *node, off_t offset, size_t size, void *buffer) {
    UNUSED(node);
    UNUSED(offset);
    return evdev_mice_read(buffer, size);
}

static ssize_t devfs_evdev_mouse_read(vfs_node_t *node, off_t offset, size_t size, void *buffer) {
    UNUSED(node);
    UNUSED(offset);
    return evdev_mouse_event_read(buffer, size);
}

static int devfs_evdev_mouse_ioctl(vfs_node_t *node, uint64_t request, uintptr_t arg) {
    UNUSED(node);
    return evdev_mouse_ioctl(request, arg);
}

static ssize_t devfs_evdev_kbd_read(vfs_node_t *node, off_t offset, size_t size, void *buffer) {
    UNUSED(node);
    UNUSED(offset);
    return evdev_kbd_event_read(buffer, size);
}

static int devfs_evdev_kbd_ioctl(vfs_node_t *node, uint64_t request, uintptr_t arg) {
    UNUSED(node);
    return evdev_kbd_ioctl(request, arg);
}

static vfs_ops_t g_mice_ops;
static vfs_ops_t g_event0_ops;
static vfs_ops_t g_event1_ops;

void evdev_init(void) {
    memset(&g_mouse_queue, 0, sizeof(g_mouse_queue));
    memset(&g_kbd_queue, 0, sizeof(g_kbd_queue));
    g_mouse_queue.lock = SPINLOCK_INIT;
    g_kbd_queue.lock = SPINLOCK_INIT;

    /* Register DevFS input nodes */
    g_mice_ops.read = devfs_evdev_mice_read;
    vfs_node_t *mice_dev = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    mice_dev->flags = VFS_TYPE_CHARDEVICE;
    mice_dev->permissions = 0666;
    mice_dev->ops = &g_mice_ops;
    devfs_register_device_path("input/mice", mice_dev);

    g_event0_ops.read = devfs_evdev_mouse_read;
    g_event0_ops.ioctl = devfs_evdev_mouse_ioctl;
    vfs_node_t *event0_dev = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    event0_dev->flags = VFS_TYPE_CHARDEVICE;
    event0_dev->permissions = 0666;
    event0_dev->ops = &g_event0_ops;
    devfs_register_device_path("input/event0", event0_dev);

    g_event1_ops.read = devfs_evdev_kbd_read;
    g_event1_ops.ioctl = devfs_evdev_kbd_ioctl;
    vfs_node_t *event1_dev = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    event1_dev->flags = VFS_TYPE_CHARDEVICE;
    event1_dev->permissions = 0666;
    event1_dev->ops = &g_event1_ops;
    devfs_register_device_path("input/event1", event1_dev);

    klog_info("Evdev: Registered /dev/input/mice, /dev/input/event0 (mouse), /dev/input/event1 (keyboard)");
}

void evdev_push_mouse_packet(int dx, int dy, int dz, uint8_t buttons) {
    bool has_events = false;

    if (dx != 0) {
        evdev_enqueue(&g_mouse_queue, EV_REL, REL_X, dx);
        has_events = true;
    }
    if (dy != 0) {
        evdev_enqueue(&g_mouse_queue, EV_REL, REL_Y, dy);
        has_events = true;
    }
    if (dz != 0) {
        evdev_enqueue(&g_mouse_queue, EV_REL, REL_WHEEL, dz);
        has_events = true;
    }

    /* Button transitions */
    uint8_t changed = buttons ^ g_last_mouse_buttons;
    if (changed & 0x01) {
        evdev_enqueue(&g_mouse_queue, EV_KEY, BTN_LEFT, (buttons & 0x01) ? 1 : 0);
        has_events = true;
    }
    if (changed & 0x02) {
        evdev_enqueue(&g_mouse_queue, EV_KEY, BTN_RIGHT, (buttons & 0x02) ? 1 : 0);
        has_events = true;
    }
    if (changed & 0x04) {
        evdev_enqueue(&g_mouse_queue, EV_KEY, BTN_MIDDLE, (buttons & 0x04) ? 1 : 0);
        has_events = true;
    }
    g_last_mouse_buttons = buttons;

    if (has_events) {
        evdev_enqueue(&g_mouse_queue, EV_SYN, SYN_REPORT, 0);
    }

    /* Also feed raw 3-byte packet to /dev/input/mice */
    spinlock_acquire(&g_mice_lock);
    if (g_mice_count + 3 <= sizeof(g_mice_buf)) {
        uint8_t flags = 0x08 | (buttons & 0x07);
        if (dx < 0) flags |= 0x10;
        if (dy < 0) flags |= 0x20;

        g_mice_buf[g_mice_tail] = flags;
        g_mice_tail = (g_mice_tail + 1) % sizeof(g_mice_buf);
        g_mice_buf[g_mice_tail] = (uint8_t)(dx & 0xFF);
        g_mice_tail = (g_mice_tail + 1) % sizeof(g_mice_buf);
        g_mice_buf[g_mice_tail] = (uint8_t)(dy & 0xFF);
        g_mice_tail = (g_mice_tail + 1) % sizeof(g_mice_buf);
        g_mice_count += 3;
    }
    spinlock_release(&g_mice_lock);
}

void evdev_push_key(uint16_t keycode, bool pressed) {
    evdev_enqueue(&g_kbd_queue, EV_KEY, keycode, pressed ? 1 : 0);
    evdev_enqueue(&g_kbd_queue, EV_SYN, SYN_REPORT, 0);
}

ssize_t evdev_mice_read(void *buf, size_t count) {
    if (!buf || count == 0)
        return 0;

    spinlock_acquire(&g_mice_lock);
    if (g_mice_count == 0) {
        spinlock_release(&g_mice_lock);
        return 0;
    }

    size_t to_read = (count < g_mice_count) ? count : g_mice_count;
    uint8_t *dst = (uint8_t *)buf;
    for (size_t i = 0; i < to_read; i++) {
        dst[i] = g_mice_buf[g_mice_head];
        g_mice_head = (g_mice_head + 1) % sizeof(g_mice_buf);
    }
    g_mice_count -= to_read;
    spinlock_release(&g_mice_lock);

    return (ssize_t)to_read;
}

ssize_t evdev_mouse_event_read(void *buf, size_t count) {
    if (!buf || count < sizeof(struct input_event))
        return 0;

    evdev_queue_t *q = &g_mouse_queue;
    spinlock_acquire(&q->lock);
    if (q->count == 0) {
        spinlock_release(&q->lock);
        return 0;
    }

    size_t max_events = count / sizeof(struct input_event);
    size_t read_events = (max_events < q->count) ? max_events : q->count;
    struct input_event *dst = (struct input_event *)buf;

    for (size_t i = 0; i < read_events; i++) {
        dst[i] = q->events[q->head];
        q->head = (q->head + 1) % EVDEV_QUEUE_SIZE;
    }
    q->count -= read_events;
    spinlock_release(&q->lock);

    return (ssize_t)(read_events * sizeof(struct input_event));
}

ssize_t evdev_kbd_event_read(void *buf, size_t count) {
    if (!buf || count < sizeof(struct input_event))
        return 0;

    evdev_queue_t *q = &g_kbd_queue;
    spinlock_acquire(&q->lock);
    if (q->count == 0) {
        spinlock_release(&q->lock);
        return 0;
    }

    size_t max_events = count / sizeof(struct input_event);
    size_t read_events = (max_events < q->count) ? max_events : q->count;
    struct input_event *dst = (struct input_event *)buf;

    for (size_t i = 0; i < read_events; i++) {
        dst[i] = q->events[q->head];
        q->head = (q->head + 1) % EVDEV_QUEUE_SIZE;
    }
    q->count -= read_events;
    spinlock_release(&q->lock);

    return (ssize_t)(read_events * sizeof(struct input_event));
}

bool evdev_mouse_has_events(void) {
    return g_mouse_queue.count > 0;
}

bool evdev_kbd_has_events(void) {
    return g_kbd_queue.count > 0;
}

bool evdev_mice_has_data(void) {
    return g_mice_count > 0;
}

int evdev_mouse_ioctl(uint64_t request, uintptr_t arg) {
    if (request == EVIOCGVERSION) {
        int *ver = (int *)arg;
        if (ver) *ver = 0x010001;
        return 0;
    } else if (request == EVIOCGID) {
        struct input_id *id = (struct input_id *)arg;
        if (id) {
            id->bustype = 0x0011; /* BUS_I8042 */
            id->vendor = 0x0001;
            id->product = 0x0001;
            id->version = 0x0100;
        }
        return 0;
    } else if ((request >> 8) == ('E' << 0) && (request & 0xFF) == 0x06) {
        /* EVIOCGNAME */
        const char *name = "SzpontOS PS/2 Mouse";
        strncpy((char *)arg, name, 64);
        return 0;
    }
    return -22;
}

int evdev_kbd_ioctl(uint64_t request, uintptr_t arg) {
    if (request == EVIOCGVERSION) {
        int *ver = (int *)arg;
        if (ver) *ver = 0x010001;
        return 0;
    } else if (request == EVIOCGID) {
        struct input_id *id = (struct input_id *)arg;
        if (id) {
            id->bustype = 0x0011; /* BUS_I8042 */
            id->vendor = 0x0001;
            id->product = 0x0002;
            id->version = 0x0100;
        }
        return 0;
    } else if ((request >> 8) == ('E' << 0) && (request & 0xFF) == 0x06) {
        /* EVIOCGNAME */
        const char *name = "SzpontOS PS/2 Keyboard";
        strncpy((char *)arg, name, 64);
        return 0;
    }
    return -22;
}
