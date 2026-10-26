/* SPDX-License-Identifier: GPL-3.0-only */
#include "control.h"

#include <stdio.h>

#include "bus.h"

#define CONTROL_NAME "org.nuros.sanar.nabu"
#define CONTROL_PATH "/org/nuros/sanar/nabu"
#define CONTROL_INTERFACE "org.nuros.sanar.Nabu"

#define NOTE_SIGNATURE "(ussssssasyb)"
#define ENTRY_SIGNATURE "(xyyssssss)"

static const char introspection_xml[] =
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection "
    "1.0//EN\" "
    "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    "  <interface name=\"" CONTROL_INTERFACE "\">\n"
    "    <method name=\"GetActive\">\n"
    "      <arg type=\"a" NOTE_SIGNATURE "\" name=\"notifications\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"GetHistory\">\n"
    "      <arg type=\"a" ENTRY_SIGNATURE "\" name=\"entries\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"ClearHistory\"/>\n"
    "    <method name=\"Dismiss\">\n"
    "      <arg type=\"u\" name=\"id\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <signal name=\"Posted\">\n"
    "      <arg type=\"" NOTE_SIGNATURE "\" name=\"notification\"/>\n"
    "    </signal>\n"
    "    <signal name=\"Closed\">\n"
    "      <arg type=\"u\" name=\"id\"/>\n"
    "      <arg type=\"u\" name=\"reason\"/>\n"
    "    </signal>\n"
    "  </interface>\n"
    "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
    "    <method name=\"Introspect\">\n"
    "      <arg type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "</node>\n";

static void append_note(DBusMessageIter *iter, const struct nabu_note *note)
{
    DBusMessageIter fields, actions;
    dbus_bool_t resident = note->resident;

    dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT, NULL, &fields);
    dbus_message_iter_append_basic(&fields, DBUS_TYPE_UINT32, &note->id);
    dbus_message_iter_append_basic(&fields, DBUS_TYPE_STRING, &note->app_name);
    dbus_message_iter_append_basic(&fields, DBUS_TYPE_STRING, &note->app_icon);
    dbus_message_iter_append_basic(&fields, DBUS_TYPE_STRING, &note->summary);
    dbus_message_iter_append_basic(&fields, DBUS_TYPE_STRING, &note->body);
    dbus_message_iter_append_basic(&fields, DBUS_TYPE_STRING, &note->desktop_entry);
    dbus_message_iter_append_basic(&fields, DBUS_TYPE_STRING, &note->image_path);
    dbus_message_iter_open_container(&fields, DBUS_TYPE_ARRAY, "s", &actions);
    for (size_t i = 0; i < note->action_count; i++)
        dbus_message_iter_append_basic(&actions, DBUS_TYPE_STRING, &note->actions[i]);
    dbus_message_iter_close_container(&fields, &actions);
    dbus_message_iter_append_basic(&fields, DBUS_TYPE_BYTE, &note->urgency);
    dbus_message_iter_append_basic(&fields, DBUS_TYPE_BOOLEAN, &resident);
    dbus_message_iter_close_container(iter, &fields);
}

static void append_entry(DBusMessageIter *iter, const struct nabu_history_entry *entry)
{
    DBusMessageIter fields;
    dbus_int64_t time = entry->time;

    dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT, NULL, &fields);
    dbus_message_iter_append_basic(&fields, DBUS_TYPE_INT64, &time);
    dbus_message_iter_append_basic(&fields, DBUS_TYPE_BYTE, &entry->urgency);
    dbus_message_iter_append_basic(&fields, DBUS_TYPE_BYTE, &entry->reason);
    dbus_message_iter_append_basic(&fields, DBUS_TYPE_STRING, &entry->app_name);
    dbus_message_iter_append_basic(&fields, DBUS_TYPE_STRING, &entry->app_icon);
    dbus_message_iter_append_basic(&fields, DBUS_TYPE_STRING, &entry->summary);
    dbus_message_iter_append_basic(&fields, DBUS_TYPE_STRING, &entry->body);
    dbus_message_iter_append_basic(&fields, DBUS_TYPE_STRING, &entry->desktop_entry);
    dbus_message_iter_append_basic(&fields, DBUS_TYPE_STRING, &entry->image_path);
    dbus_message_iter_close_container(iter, &fields);
}

static DBusMessage *reply_active(struct nabud_bus *bus, DBusMessage *message)
{
    DBusMessage *reply = dbus_message_new_method_return(message);
    DBusMessageIter iter, array;

    if (!reply)
        return NULL;
    dbus_message_iter_init_append(reply, &iter);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, NOTE_SIGNATURE, &array);
    for (size_t i = 0; i < bus->notes.count; i++)
        append_note(&array, &bus->notes.items[i]);
    dbus_message_iter_close_container(&iter, &array);
    return reply;
}

static DBusMessage *reply_history(struct nabud_bus *bus, DBusMessage *message)
{
    DBusMessage *reply = dbus_message_new_method_return(message);
    DBusMessageIter iter, array;

    if (!reply)
        return NULL;
    dbus_message_iter_init_append(reply, &iter);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, ENTRY_SIGNATURE, &array);
    for (size_t i = 0; i < bus->history.count; i++)
        append_entry(&array, &bus->history.items[i]);
    dbus_message_iter_close_container(&iter, &array);
    return reply;
}

static DBusMessage *call_clear_history(struct nabud_bus *bus, DBusMessage *message)
{
    if (nabu_history_clear(&bus->history) != 0)
        fprintf(stderr, "nabud: cannot save history\n");
    return dbus_message_new_method_return(message);
}

static DBusMessage *call_dismiss(struct nabud_bus *bus, DBusMessage *message)
{
    uint32_t id;

    if (!dbus_message_get_args(message, NULL, DBUS_TYPE_UINT32, &id, DBUS_TYPE_INVALID))
        return dbus_message_new_error(message, DBUS_ERROR_INVALID_ARGS,
                                      "Expected one unsigned integer");
    nabud_bus_close_note(bus, id, NABU_CLOSE_DISMISSED);
    return dbus_message_new_method_return(message);
}

static DBusMessage *reply_introspect(DBusMessage *message)
{
    DBusMessage *reply = dbus_message_new_method_return(message);
    const char *xml = introspection_xml;

    if (reply)
        dbus_message_append_args(reply, DBUS_TYPE_STRING, &xml, DBUS_TYPE_INVALID);
    return reply;
}

static DBusHandlerResult handle_message(DBusConnection *connection,
                                        DBusMessage *message, void *data)
{
    struct nabud_bus *bus = data;
    DBusMessage *reply;

    if (dbus_message_is_method_call(message, CONTROL_INTERFACE, "GetActive"))
        reply = reply_active(bus, message);
    else if (dbus_message_is_method_call(message, CONTROL_INTERFACE, "GetHistory"))
        reply = reply_history(bus, message);
    else if (dbus_message_is_method_call(message, CONTROL_INTERFACE, "ClearHistory"))
        reply = call_clear_history(bus, message);
    else if (dbus_message_is_method_call(message, CONTROL_INTERFACE, "Dismiss"))
        reply = call_dismiss(bus, message);
    else if (dbus_message_is_method_call(message, "org.freedesktop.DBus.Introspectable",
                                         "Introspect"))
        reply = reply_introspect(message);
    else
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (!reply)
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    dbus_connection_send(connection, reply, NULL);
    dbus_message_unref(reply);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static const DBusObjectPathVTable vtable = {
    .message_function = handle_message,
};

int nabud_control_open(struct nabud_bus *bus)
{
    DBusError error;

    dbus_error_init(&error);
    if (dbus_bus_request_name(bus->connection, CONTROL_NAME, DBUS_NAME_FLAG_DO_NOT_QUEUE,
                              &error) != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        fprintf(stderr, "nabud: cannot own %s%s%s\n", CONTROL_NAME,
                dbus_error_is_set(&error) ? ": " : "",
                dbus_error_is_set(&error) ? error.message : "");
        dbus_error_free(&error);
        return -1;
    }
    if (!dbus_connection_register_object_path(bus->connection, CONTROL_PATH, &vtable, bus)) {
        fprintf(stderr, "nabud: cannot register D-Bus object\n");
        return -1;
    }
    return 0;
}

void nabud_control_posted(struct nabud_bus *bus, uint32_t id)
{
    const struct nabu_note *note = nabu_notes_find(&bus->notes, id);
    DBusMessage *signal;
    DBusMessageIter iter;

    if (!bus->connection || !note)
        return;
    signal = dbus_message_new_signal(CONTROL_PATH, CONTROL_INTERFACE, "Posted");
    if (!signal)
        return;
    dbus_message_iter_init_append(signal, &iter);
    append_note(&iter, note);
    dbus_connection_send(bus->connection, signal, NULL);
    dbus_message_unref(signal);
}

void nabud_control_closed(struct nabud_bus *bus, uint32_t id, uint32_t reason)
{
    DBusMessage *signal;

    if (!bus->connection)
        return;
    signal = dbus_message_new_signal(CONTROL_PATH, CONTROL_INTERFACE, "Closed");
    if (!signal)
        return;
    dbus_message_append_args(signal, DBUS_TYPE_UINT32, &id, DBUS_TYPE_UINT32, &reason,
                             DBUS_TYPE_INVALID);
    dbus_connection_send(bus->connection, signal, NULL);
    dbus_message_unref(signal);
}
