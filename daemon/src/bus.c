/* SPDX-License-Identifier: GPL-3.0-only */
#include "bus.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

#include "control.h"

#define BUS_NAME "org.freedesktop.Notifications"
#define OBJECT_PATH "/org/freedesktop/Notifications"
#define INTERFACE "org.freedesktop.Notifications"
#define HISTORY_LIMIT 200

static const char *const capabilities[] = {"actions", "body"};

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
    "    <method name=\"Notify\">\n"
    "      <arg type=\"s\" name=\"app_name\" direction=\"in\"/>\n"
    "      <arg type=\"u\" name=\"replaces_id\" direction=\"in\"/>\n"
    "      <arg type=\"s\" name=\"app_icon\" direction=\"in\"/>\n"
    "      <arg type=\"s\" name=\"summary\" direction=\"in\"/>\n"
    "      <arg type=\"s\" name=\"body\" direction=\"in\"/>\n"
    "      <arg type=\"as\" name=\"actions\" direction=\"in\"/>\n"
    "      <arg type=\"a{sv}\" name=\"hints\" direction=\"in\"/>\n"
    "      <arg type=\"i\" name=\"expire_timeout\" direction=\"in\"/>\n"
    "      <arg type=\"u\" name=\"id\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"CloseNotification\">\n"
    "      <arg type=\"u\" name=\"id\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <signal name=\"NotificationClosed\">\n"
    "      <arg type=\"u\" name=\"id\"/>\n"
    "      <arg type=\"u\" name=\"reason\"/>\n"
    "    </signal>\n"
    "    <signal name=\"ActionInvoked\">\n"
    "      <arg type=\"u\" name=\"id\"/>\n"
    "      <arg type=\"s\" name=\"action_key\"/>\n"
    "    </signal>\n"
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

static void arm_timer(struct nabud_bus *bus)
{
    int64_t next = nabu_notes_next_expiry(&bus->notes);
    struct itimerspec spec = {0};

    if (next >= 0) {
        spec.it_value.tv_sec = next / 1000;
        spec.it_value.tv_nsec = (next % 1000) * 1000000;
    }
    timerfd_settime(bus->timer_fd, TFD_TIMER_ABSTIME, &spec, NULL);
}

static void emit_closed(struct nabud_bus *bus, uint32_t id, uint32_t reason)
{
    DBusMessage *signal;

    if (!bus->connection)
        return;
    signal = dbus_message_new_signal(OBJECT_PATH, INTERFACE, "NotificationClosed");
    if (!signal)
        return;
    dbus_message_append_args(signal, DBUS_TYPE_UINT32, &id, DBUS_TYPE_UINT32, &reason,
                             DBUS_TYPE_INVALID);
    dbus_connection_send(bus->connection, signal, NULL);
    dbus_message_unref(signal);
}

bool nabud_bus_close_note(struct nabud_bus *bus, uint32_t id, uint32_t reason)
{
    const struct nabu_note *note = nabu_notes_find(&bus->notes, id);

    if (!note)
        return false;
    if (!note->transient &&
        nabu_history_add(&bus->history, note, (uint8_t)reason, time(NULL)) != 0)
        fprintf(stderr, "nabud: cannot save history\n");
    nabu_notes_close(&bus->notes, id);
    arm_timer(bus);
    emit_closed(bus, id, reason);
    nabud_control_closed(bus, id, reason);
    return true;
}

static void emit_action(struct nabud_bus *bus, uint32_t id, const char *key)
{
    DBusMessage *signal;

    if (!bus->connection)
        return;
    signal = dbus_message_new_signal(OBJECT_PATH, INTERFACE, "ActionInvoked");
    if (!signal)
        return;
    dbus_message_append_args(signal, DBUS_TYPE_UINT32, &id, DBUS_TYPE_STRING, &key,
                             DBUS_TYPE_INVALID);
    dbus_connection_send(bus->connection, signal, NULL);
    dbus_message_unref(signal);
}

bool nabud_bus_invoke(struct nabud_bus *bus, uint32_t id, const char *key)
{
    const struct nabu_note *note = nabu_notes_find(&bus->notes, id);
    bool found = false, resident;

    if (!note)
        return false;
    for (size_t i = 0; i + 1 < note->action_count && !found; i += 2)
        found = strcmp(note->actions[i], key) == 0;
    if (!found)
        return false;
    resident = note->resident;
    emit_action(bus, id, key);
    if (!resident)
        nabud_bus_close_note(bus, id, NABU_CLOSE_DISMISSED);
    return true;
}

static bool read_basic(DBusMessageIter *iter, int type, void *out)
{
    if (dbus_message_iter_get_arg_type(iter) != type)
        return false;
    dbus_message_iter_get_basic(iter, out);
    dbus_message_iter_next(iter);
    return true;
}

static bool parse_actions(DBusMessageIter *iter, struct nabu_note_input *input)
{
    DBusMessageIter array;
    const char **list = NULL;
    size_t count = 0, capacity = 0;

    if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY ||
        dbus_message_iter_get_element_type(iter) != DBUS_TYPE_STRING)
        return false;
    dbus_message_iter_recurse(iter, &array);
    while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_STRING) {
        if (count == capacity) {
            size_t grown = capacity ? capacity * 2 : 8;
            const char **resized = realloc(list, grown * sizeof(*list));

            if (!resized) {
                free(list);
                return false;
            }
            list = resized;
            capacity = grown;
        }
        dbus_message_iter_get_basic(&array, &list[count++]);
        dbus_message_iter_next(&array);
    }
    dbus_message_iter_next(iter);
    input->actions = list;
    input->action_count = count;
    return true;
}

static void apply_hint(struct nabu_note_input *input, const char *key,
                       DBusMessageIter *value)
{
    int type = dbus_message_iter_get_arg_type(value);

    if (type == DBUS_TYPE_BYTE && strcmp(key, "urgency") == 0) {
        unsigned char urgency;

        dbus_message_iter_get_basic(value, &urgency);
        input->urgency = urgency <= NABU_URGENCY_CRITICAL ? urgency : NABU_URGENCY_NORMAL;
    } else if (type == DBUS_TYPE_BOOLEAN && strcmp(key, "transient") == 0) {
        dbus_bool_t flag;

        dbus_message_iter_get_basic(value, &flag);
        input->transient = flag;
    } else if (type == DBUS_TYPE_BOOLEAN && strcmp(key, "resident") == 0) {
        dbus_bool_t flag;

        dbus_message_iter_get_basic(value, &flag);
        input->resident = flag;
    } else if (type == DBUS_TYPE_STRING && strcmp(key, "desktop-entry") == 0) {
        dbus_message_iter_get_basic(value, &input->desktop_entry);
    } else if (type == DBUS_TYPE_STRING && strcmp(key, "image-path") == 0) {
        dbus_message_iter_get_basic(value, &input->image_path);
    }
}

static bool parse_hints(DBusMessageIter *iter, struct nabu_note_input *input)
{
    DBusMessageIter dict;

    if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY ||
        dbus_message_iter_get_element_type(iter) != DBUS_TYPE_DICT_ENTRY)
        return false;
    dbus_message_iter_recurse(iter, &dict);
    while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
        DBusMessageIter entry, value;
        const char *key;

        dbus_message_iter_recurse(&dict, &entry);
        if (dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_STRING) {
            dbus_message_iter_get_basic(&entry, &key);
            dbus_message_iter_next(&entry);
            if (dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_VARIANT) {
                dbus_message_iter_recurse(&entry, &value);
                apply_hint(input, key, &value);
            }
        }
        dbus_message_iter_next(&dict);
    }
    dbus_message_iter_next(iter);
    return true;
}

static bool parse_notify(DBusMessage *message, uint32_t *replaces_id,
                         struct nabu_note_input *input)
{
    DBusMessageIter iter;

    if (!dbus_message_iter_init(message, &iter))
        return false;
    if (!(read_basic(&iter, DBUS_TYPE_STRING, &input->app_name) &&
          read_basic(&iter, DBUS_TYPE_UINT32, replaces_id) &&
          read_basic(&iter, DBUS_TYPE_STRING, &input->app_icon) &&
          read_basic(&iter, DBUS_TYPE_STRING, &input->summary) &&
          read_basic(&iter, DBUS_TYPE_STRING, &input->body) &&
          parse_actions(&iter, input)))
        return false;
    if (parse_hints(&iter, input) &&
        read_basic(&iter, DBUS_TYPE_INT32, &input->timeout_ms))
        return true;
    free((void *)input->actions);
    input->actions = NULL;
    return false;
}

static DBusMessage *call_notify(struct nabud_bus *bus, DBusMessage *message)
{
    struct nabu_note_input input = {.urgency = NABU_URGENCY_NORMAL};
    uint32_t replaces_id, id;
    DBusMessage *reply;

    if (!parse_notify(message, &replaces_id, &input))
        return dbus_message_new_error(message, DBUS_ERROR_INVALID_ARGS,
                                      "Expected susssasa{sv}i");
    id = nabu_notes_post(&bus->notes, replaces_id, &input, nabu_now_ms());
    free((void *)input.actions);
    if (id == 0)
        return dbus_message_new_error(message, DBUS_ERROR_NO_MEMORY, "Out of memory");
    arm_timer(bus);
    if (bus->do_not_disturb && input.urgency != NABU_URGENCY_CRITICAL)
        bus->suppressed_id = id;
    else
        nabud_control_posted(bus, id);
    reply = dbus_message_new_method_return(message);
    if (reply)
        dbus_message_append_args(reply, DBUS_TYPE_UINT32, &id, DBUS_TYPE_INVALID);
    return reply;
}

static DBusMessage *call_close(struct nabud_bus *bus, DBusMessage *message)
{
    uint32_t id;

    if (!dbus_message_get_args(message, NULL, DBUS_TYPE_UINT32, &id, DBUS_TYPE_INVALID))
        return dbus_message_new_error(message, DBUS_ERROR_INVALID_ARGS,
                                      "Expected one unsigned integer");
    nabud_bus_close_note(bus, id, NABU_CLOSE_CALLED);
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

    if (dbus_message_is_method_call(message, INTERFACE, "Notify"))
        reply = call_notify(bus, message);
    else if (dbus_message_is_method_call(message, INTERFACE, "CloseNotification"))
        reply = call_close(bus, message);
    else if (dbus_message_is_method_call(message, INTERFACE, "GetCapabilities"))
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
    if (bus->suppressed_id) {
        uint32_t id = bus->suppressed_id;

        bus->suppressed_id = 0;
        nabud_bus_close_note(bus, id, NABU_CLOSE_UNDEFINED);
    }
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

int nabud_bus_open(struct nabud_bus *bus, int epfd, int timer_fd,
                   const char *history_path)
{
    DBusError error;

    bus->connection = NULL;
    bus->epfd = epfd;
    bus->timer_fd = timer_fd;
    nabu_notes_init(&bus->notes);
    if (nabu_history_open(&bus->history, history_path, HISTORY_LIMIT) != 0) {
        fprintf(stderr, "nabud: out of memory\n");
        return -1;
    }

    dbus_error_init(&error);
    bus->connection = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
    if (!bus->connection) {
        fprintf(stderr, "nabud: session bus: %s\n", error.message);
        dbus_error_free(&error);
        nabud_bus_close(bus);
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
    if (nabud_control_open(bus) != 0)
        goto fail;
    dispatch_all(bus);
    return 0;

fail:
    nabud_bus_close(bus);
    return -1;
}

void nabud_bus_close(struct nabud_bus *bus)
{
    nabu_notes_free(&bus->notes);
    nabu_history_close(&bus->history);
    if (!bus->connection)
        return;
    dbus_connection_close(bus->connection);
    dbus_connection_unref(bus->connection);
    bus->connection = NULL;
}

void nabud_bus_expire(struct nabud_bus *bus)
{
    int64_t now = nabu_now_ms();
    uint64_t ticks;
    uint32_t id;

    if (read(bus->timer_fd, &ticks, sizeof(ticks)) != sizeof(ticks))
        return;
    while ((id = nabu_notes_next_expired(&bus->notes, now)) != 0)
        nabud_bus_close_note(bus, id, NABU_CLOSE_EXPIRED);
    arm_timer(bus);
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
