/* SPDX-License-Identifier: GPL-3.0-only */
#include "client.h"

#include <stdio.h>
#include <string.h>

#define DESTINATION "org.nuros.sanar.nabu"
#define OBJECT_PATH "/org/nuros/sanar/nabu"
#define INTERFACE "org.nuros.sanar.Nabu"

int nabu_client_open(struct nabu_client *client)
{
    DBusError error;

    dbus_error_init(&error);
    client->connection = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
    if (!client->connection) {
        fprintf(stderr, "nabu: session bus: %s\n", error.message);
        dbus_error_free(&error);
        return -1;
    }
    dbus_connection_set_exit_on_disconnect(client->connection, FALSE);
    return 0;
}

void nabu_client_close(struct nabu_client *client)
{
    if (!client->connection)
        return;
    dbus_connection_close(client->connection);
    dbus_connection_unref(client->connection);
    client->connection = NULL;
}

DBusMessage *nabu_client_method(const char *method)
{
    return dbus_message_new_method_call(DESTINATION, OBJECT_PATH, INTERFACE, method);
}

DBusMessage *nabu_client_properties_method(const char *method)
{
    return dbus_message_new_method_call(DESTINATION, OBJECT_PATH,
                                        "org.freedesktop.DBus.Properties", method);
}

DBusMessage *nabu_client_call(struct nabu_client *client, DBusMessage *call)
{
    DBusError error;
    DBusMessage *reply;

    if (!call) {
        fprintf(stderr, "nabu: out of memory\n");
        return NULL;
    }
    dbus_error_init(&error);
    reply = dbus_connection_send_with_reply_and_block(client->connection, call, 5000, &error);
    dbus_message_unref(call);
    if (reply)
        return reply;
    if (strcmp(error.name, DBUS_ERROR_SERVICE_UNKNOWN) == 0 ||
        strcmp(error.name, DBUS_ERROR_NAME_HAS_NO_OWNER) == 0)
        fprintf(stderr, "nabu: the daemon is not running\n");
    else
        fprintf(stderr, "nabu: %s\n", error.message);
    dbus_error_free(&error);
    return NULL;
}
