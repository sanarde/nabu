/* SPDX-License-Identifier: GPL-3.0-only */
#include "bus.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>

#define BUS_NAME "org.freedesktop.Notifications"
#define OBJECT_PATH "/org/freedesktop/Notifications"
#define INTERFACE "org.freedesktop.Notifications"

static const char *const capabilities[] = {"body"};

static const char introspection_xml[] =
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection "
    "1.0//EN\" "
    "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    "  <interface name=\"" INTERFACE "\">\n"
    "    <method name=\"GetCapabilities\">\n"
    "      <arg type=\"as\" name=\"capabilities\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"GetServerInformation\">\n"
    "      <arg type=\"s\" name=\"name\" direction=\"out\"/>\n"
    "      <arg type=\"s\" name=\"vendor\" direction=\"out\"/>\n"
    "      <arg type=\"s\" name=\"version\" direction=\"out\"/>\n"
    "      <arg type=\"s\" name=\"spec_version\" direction=\"out\"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
    "    <method name=\"Introspect\">\n"
    "      <arg type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "</node>\n";

static int watch_update(struct nabud_bus *bus, struct nabud_bus_watch *w, int op)
{
    struct epoll_event ev = {.events = 0, .data.ptr = &w->source};

    if (dbus_watch_get_enabled(w->watch)) {
        unsigned int flags = dbus_watch_get_flags(w->watch);

        if (flags & DBUS_WATCH_READABLE)
            ev.events |= EPOLLIN;
        if (flags & DBUS_WATCH_WRITABLE)
            ev.events |= EPOLLOUT;
    }
    return epoll_ctl(bus->epfd, op, w->fd, &ev);
}

static dbus_bool_t add_watch(DBusWatch *watch, void *data)
{
    struct nabud_bus *bus = data;
    struct nabud_bus_watch *w = calloc(1, sizeof(*w));

    if (!w)
        return FALSE;
    w->source.kind = NABUD_SRC_BUS;
    w->watch = watch;
    w->fd = fcntl(dbus_watch_get_unix_fd(watch), F_DUPFD_CLOEXEC, 0);
    if (w->fd < 0) {
        free(w);
        return FALSE;
    }
    if (watch_update(bus, w, EPOLL_CTL_ADD) != 0) {
        close(w->fd);
        free(w);
        return FALSE;
    }
    dbus_watch_set_data(watch, w, NULL);
    return TRUE;
}

static void remove_watch(DBusWatch *watch, void *data)
{
    struct nabud_bus *bus = data;
    struct nabud_bus_watch *w = dbus_watch_get_data(watch);

    epoll_ctl(bus->epfd, EPOLL_CTL_DEL, w->fd, NULL);
    close(w->fd);
    dbus_watch_set_data(watch, NULL, NULL);
    free(w);
}

static void toggle_watch(DBusWatch *watch, void *data)
{
    watch_update(data, dbus_watch_get_data(watch), EPOLL_CTL_MOD);
}

static DBusMessage *reply_capabilities(DBusMessage *message)
{
    DBusMessage *reply = dbus_message_new_method_return(message);
    const char *const *list = capabilities;
    int count = (int)(sizeof(capabilities) / sizeof(capabilities[0]));

    if (reply)
        dbus_message_append_args(reply, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &list, count,
                                 DBUS_TYPE_INVALID);
    return reply;
}

static DBusMessage *reply_server_information(DBusMessage *message)
{
    DBusMessage *reply = dbus_message_new_method_return(message);
    const char *name = "Nabu", *vendor = "SanarDE", *version = NABU_VERSION,
               *spec = "1.3";

    if (reply)
        dbus_message_append_args(reply, DBUS_TYPE_STRING, &name, DBUS_TYPE_STRING,
                                 &vendor, DBUS_TYPE_STRING, &version, DBUS_TYPE_STRING,
                                 &spec, DBUS_TYPE_INVALID);
    return reply;
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
    DBusMessage *reply;

    (void)data;
    if (dbus_message_is_method_call(message, INTERFACE, "GetCapabilities"))
        reply = reply_capabilities(message);
    else if (dbus_message_is_method_call(message, INTERFACE, "GetServerInformation"))
        reply = reply_server_information(message);
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

static void dispatch_all(struct nabud_bus *bus)
{
    while (dbus_connection_get_dispatch_status(bus->connection) == DBUS_DISPATCH_DATA_REMAINS)
        dbus_connection_dispatch(bus->connection);
}

int nabud_bus_open(struct nabud_bus *bus, int epfd)
{
    DBusError error;

    bus->connection = NULL;
    bus->epfd = epfd;

    dbus_error_init(&error);
    bus->connection = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
    if (!bus->connection) {
        fprintf(stderr, "nabud: session bus: %s\n", error.message);
        dbus_error_free(&error);
        return -1;
    }
    dbus_connection_set_exit_on_disconnect(bus->connection, FALSE);

    if (dbus_bus_request_name(bus->connection, BUS_NAME, DBUS_NAME_FLAG_DO_NOT_QUEUE,
                              &error) != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        fprintf(stderr, "nabud: cannot own %s%s%s\n", BUS_NAME,
                dbus_error_is_set(&error) ? ": " : "",
                dbus_error_is_set(&error) ? error.message : "");
        dbus_error_free(&error);
        goto fail;
    }

    if (!dbus_connection_register_object_path(bus->connection, OBJECT_PATH, &vtable, bus) ||
        !dbus_connection_set_watch_functions(bus->connection, add_watch, remove_watch,
                                             toggle_watch, bus, NULL)) {
        fprintf(stderr, "nabud: cannot register D-Bus object\n");
        goto fail;
    }
    dispatch_all(bus);
    return 0;

fail:
    dbus_connection_close(bus->connection);
    dbus_connection_unref(bus->connection);
    bus->connection = NULL;
    return -1;
}

void nabud_bus_close(struct nabud_bus *bus)
{
    if (!bus->connection)
        return;
    dbus_connection_close(bus->connection);
    dbus_connection_unref(bus->connection);
    bus->connection = NULL;
}

void nabud_bus_event(struct nabud_bus *bus, struct nabud_bus_watch *watch,
                     uint32_t events)
{
    unsigned int flags = 0;

    if (!bus->connection || !dbus_watch_get_enabled(watch->watch))
        return;
    if (events & EPOLLIN)
        flags |= DBUS_WATCH_READABLE;
    if (events & EPOLLOUT)
        flags |= DBUS_WATCH_WRITABLE;
    if (events & EPOLLERR)
        flags |= DBUS_WATCH_ERROR;
    if (events & EPOLLHUP)
        flags |= DBUS_WATCH_HANGUP;

    dbus_watch_handle(watch->watch, flags);
    dispatch_all(bus);

    if (!dbus_connection_get_is_connected(bus->connection)) {
        fprintf(stderr, "nabud: disconnected from session bus\n");
        nabud_bus_close(bus);
    }
}
