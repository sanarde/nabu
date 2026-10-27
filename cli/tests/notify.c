/* SPDX-License-Identifier: GPL-3.0-only */
#include <dbus/dbus.h>
#include <stdio.h>
#include <stdlib.h>

static void append_hint(DBusMessageIter *dict, const char *key, int type,
                        const char *signature, const void *value)
{
    DBusMessageIter entry, variant;

    dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, signature, &variant);
    dbus_message_iter_append_basic(&variant, type, value);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(dict, &entry);
}

int main(int argc, char **argv)
{
    const char *app = "nabu-test", *icon = "", *body = "body";
    const char *action_key = "default", *action_label = "Open";
    DBusConnection *connection;
    DBusMessage *message, *reply;
    DBusMessageIter iter, array;
    dbus_uint32_t replaces_id = 0, id = 0;
    dbus_int32_t timeout = argc > 2 ? atoi(argv[2]) : 0;
    unsigned char urgency = argc > 3 ? (unsigned char)atoi(argv[3]) : 1;
    dbus_bool_t transient = argc > 4 ? atoi(argv[4]) != 0 : FALSE;

    if (argc < 2)
        return 2;
    connection = dbus_bus_get_private(DBUS_BUS_SESSION, NULL);
    if (!connection)
        return 1;
    message = dbus_message_new_method_call("org.freedesktop.Notifications",
                                           "/org/freedesktop/Notifications",
                                           "org.freedesktop.Notifications", "Notify");
    dbus_message_append_args(message, DBUS_TYPE_STRING, &app, DBUS_TYPE_UINT32,
                             &replaces_id, DBUS_TYPE_STRING, &icon, DBUS_TYPE_STRING,
                             &argv[1], DBUS_TYPE_STRING, &body, DBUS_TYPE_INVALID);
    dbus_message_iter_init_append(message, &iter);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "s", &array);
    dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &action_key);
    dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &action_label);
    dbus_message_iter_close_container(&iter, &array);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &array);
    append_hint(&array, "urgency", DBUS_TYPE_BYTE, "y", &urgency);
    append_hint(&array, "transient", DBUS_TYPE_BOOLEAN, "b", &transient);
    dbus_message_iter_close_container(&iter, &array);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &timeout);

    reply = dbus_connection_send_with_reply_and_block(connection, message, 5000, NULL);
    if (!reply || !dbus_message_get_args(reply, NULL, DBUS_TYPE_UINT32, &id,
                                         DBUS_TYPE_INVALID))
        return 1;
    printf("%u\n", id);
    dbus_message_unref(reply);
    dbus_message_unref(message);
    dbus_connection_close(connection);
    dbus_connection_unref(connection);
    return 0;
}
