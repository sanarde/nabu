/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NABUD_BUS_H
#define NABUD_BUS_H

#include <dbus/dbus.h>
#include <stdint.h>

#include "loop.h"
#include "notes.h"

struct nabud_bus_watch {
    struct nabud_source source;
    DBusWatch *watch;
    int fd;
};

struct nabud_bus {
    DBusConnection *connection;
    int epfd;
    int timer_fd;
    struct nabu_notes notes;
};

int nabud_bus_open(struct nabud_bus *bus, int epfd, int timer_fd);
void nabud_bus_close(struct nabud_bus *bus);
void nabud_bus_event(struct nabud_bus *bus, struct nabud_bus_watch *watch,
                     uint32_t events);
void nabud_bus_expire(struct nabud_bus *bus);

#endif
