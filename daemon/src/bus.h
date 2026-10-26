/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NABUD_BUS_H
#define NABUD_BUS_H

#include <dbus/dbus.h>
#include <stdbool.h>
#include <stdint.h>

#include "loop.h"
#include "history.h"
#include "notes.h"

struct nabud_bus_watch {
    struct nabud_source source;
    DBusWatch *watch;
    int fd;
};

struct nabud_config;

struct nabud_bus {
    DBusConnection *connection;
    int epfd;
    int timer_fd;
    struct nabu_notes notes;
    struct nabu_history history;
    struct nabud_config *config;
    bool do_not_disturb;
    uint32_t suppressed_id;
};

int nabud_bus_open(struct nabud_bus *bus, int epfd, int timer_fd,
                   const char *history_path);
void nabud_bus_close(struct nabud_bus *bus);
void nabud_bus_event(struct nabud_bus *bus, struct nabud_bus_watch *watch,
                     uint32_t events);
void nabud_bus_expire(struct nabud_bus *bus);

/* Moves the notification to the history, closes it and emits the signals. */
bool nabud_bus_close_note(struct nabud_bus *bus, uint32_t id, uint32_t reason);

/*
 * Emits ActionInvoked for an action of the notification and closes it unless
 * it is resident. Returns false when the notification or the action is unknown.
 */
bool nabud_bus_invoke(struct nabud_bus *bus, uint32_t id, const char *key);

#endif
