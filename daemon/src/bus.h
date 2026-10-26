/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NABUD_BUS_H
#define NABUD_BUS_H

#include <dbus/dbus.h>
#include <stdint.h>

#include "loop.h"

struct nabud_bus_watch {
    struct nabud_source source;
    DBusWatch *watch;
    int fd;
};

struct nabud_bus {
    DBusConnection *connection;
    int epfd;
};

int nabud_bus_open(struct nabud_bus *bus, int epfd);
void nabud_bus_close(struct nabud_bus *bus);
void nabud_bus_event(struct nabud_bus *bus, struct nabud_bus_watch *watch,
                     uint32_t events);

#endif
