/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NABU_CLIENT_H
#define NABU_CLIENT_H

#include <dbus/dbus.h>

struct nabu_client {
    DBusConnection *connection;
};

int nabu_client_open(struct nabu_client *client);
void nabu_client_close(struct nabu_client *client);

/* Creates a call to a method of org.nuros.sanar.Nabu. */
DBusMessage *nabu_client_method(const char *method);

/* Creates a call to a method of org.freedesktop.DBus.Properties on the daemon. */
DBusMessage *nabu_client_properties_method(const char *method);

/*
 * Sends the call and consumes it. Returns the reply, or NULL after printing
 * the reason to stderr.
 */
DBusMessage *nabu_client_call(struct nabu_client *client, DBusMessage *call);

#endif
