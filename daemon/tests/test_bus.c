/* SPDX-License-Identifier: GPL-3.0-only */
#include <dbus/dbus.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "check.h"

#define NAME "org.freedesktop.Notifications"
#define PATH "/org/freedesktop/Notifications"
#define IFACE "org.freedesktop.Notifications"

static DBusMessage *call(DBusConnection *c, const char *method)
{
    DBusMessage *message = dbus_message_new_method_call(NAME, PATH, IFACE, method);
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(c, message, 5000, NULL);

    dbus_message_unref(message);
    return reply;
}

static int wait_for_name(DBusConnection *c)
{
    for (int i = 0; i < 100; i++) {
        if (dbus_bus_name_has_owner(c, NAME, NULL))
            return 1;
        usleep(50000);
    }
    return 0;
}

static void check_server_information(DBusConnection *c)
{
    DBusMessage *reply = call(c, "GetServerInformation");
    const char *name = NULL, *vendor = NULL, *version = NULL, *spec = NULL;

    CHECK(reply != NULL);
    if (!reply)
        return;
    CHECK(dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING, &name, DBUS_TYPE_STRING,
                                &vendor, DBUS_TYPE_STRING, &version, DBUS_TYPE_STRING,
                                &spec, DBUS_TYPE_INVALID));
    CHECK_STR(name, "Nabu");
    CHECK_STR(vendor, "SanarDE");
    CHECK(version && version[0]);
    CHECK_STR(spec, "1.3");
    dbus_message_unref(reply);
}

static void check_capabilities(DBusConnection *c)
{
    DBusMessage *reply = call(c, "GetCapabilities");
    const char **list = NULL;
    int count = 0;
    int body = 0;

    CHECK(reply != NULL);
    if (!reply)
        return;
    CHECK(dbus_message_get_args(reply, NULL, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &list,
                                &count, DBUS_TYPE_INVALID));
    for (int i = 0; i < count; i++)
        body |= strcmp(list[i], "body") == 0;
    CHECK(body);
    dbus_free_string_array((char **)list);
    dbus_message_unref(reply);
}

static void check_introspection(DBusConnection *c)
{
    DBusMessage *message = dbus_message_new_method_call(
        NAME, PATH, "org.freedesktop.DBus.Introspectable", "Introspect");
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(c, message, 5000, NULL);
    const char *xml = NULL;

    dbus_message_unref(message);
    CHECK(reply != NULL);
    if (!reply)
        return;
    CHECK(dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING, &xml, DBUS_TYPE_INVALID));
    CHECK(xml && strstr(xml, "GetServerInformation"));
    dbus_message_unref(reply);
}

static uint32_t notify(DBusConnection *c, uint32_t replaces_id, const char *summary,
                       int32_t timeout_ms)
{
    DBusMessage *message = dbus_message_new_method_call(NAME, PATH, IFACE, "Notify");
    DBusMessageIter iter, array;
    DBusMessage *reply;
    const char *app = "test", *icon = "", *body = "body";
    uint32_t id = 0;

    dbus_message_append_args(message, DBUS_TYPE_STRING, &app, DBUS_TYPE_UINT32,
                             &replaces_id, DBUS_TYPE_STRING, &icon, DBUS_TYPE_STRING,
                             &summary, DBUS_TYPE_STRING, &body, DBUS_TYPE_INVALID);
    dbus_message_iter_init_append(message, &iter);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "s", &array);
    dbus_message_iter_close_container(&iter, &array);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &array);
    dbus_message_iter_close_container(&iter, &array);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &timeout_ms);

    reply = dbus_connection_send_with_reply_and_block(c, message, 5000, NULL);
    dbus_message_unref(message);
    CHECK(reply != NULL);
    if (reply) {
        CHECK(dbus_message_get_args(reply, NULL, DBUS_TYPE_UINT32, &id, DBUS_TYPE_INVALID));
        dbus_message_unref(reply);
    }
    return id;
}

static void close_notification(DBusConnection *c, uint32_t id)
{
    DBusMessage *message = dbus_message_new_method_call(NAME, PATH, IFACE, "CloseNotification");
    DBusMessage *reply;

    dbus_message_append_args(message, DBUS_TYPE_UINT32, &id, DBUS_TYPE_INVALID);
    reply = dbus_connection_send_with_reply_and_block(c, message, 5000, NULL);
    dbus_message_unref(message);
    CHECK(reply != NULL);
    if (reply)
        dbus_message_unref(reply);
}

static int wait_closed(DBusConnection *c, uint32_t id, uint32_t *reason, int timeout_ms)
{
    for (int waited = 0; waited <= timeout_ms; waited += 50) {
        DBusMessage *message;

        dbus_connection_read_write(c, 50);
        while ((message = dbus_connection_pop_message(c))) {
            uint32_t got_id, got_reason;

            if (dbus_message_is_signal(message, IFACE, "NotificationClosed") &&
                dbus_message_get_args(message, NULL, DBUS_TYPE_UINT32, &got_id,
                                      DBUS_TYPE_UINT32, &got_reason, DBUS_TYPE_INVALID) &&
                got_id == id) {
                *reason = got_reason;
                dbus_message_unref(message);
                return 1;
            }
            dbus_message_unref(message);
        }
    }
    return 0;
}

static void check_notifications(DBusConnection *c)
{
    uint32_t a, b, reason = 0;

    dbus_bus_add_match(c, "type='signal',interface='" IFACE "'", NULL);

    a = notify(c, 0, "first", 0);
    CHECK(a != 0);
    b = notify(c, 0, "second", 0);
    CHECK(b != 0 && b != a);
    CHECK(notify(c, a, "first again", 0) == a);

    close_notification(c, a);
    CHECK(wait_closed(c, a, &reason, 2000));
    CHECK(reason == 3);

    close_notification(c, 9999);

    b = notify(c, 0, "short", 100);
    CHECK(wait_closed(c, b, &reason, 2000));
    CHECK(reason == 1);
}

static pid_t spawn(const char *path)
{
    pid_t pid = fork();

    if (pid == 0) {
        execl(path, path, (char *)NULL);
        _exit(127);
    }
    return pid;
}

int main(int argc, char **argv)
{
    DBusConnection *c;
    pid_t daemon, second;
    int status;

    if (argc != 2)
        return 1;
    c = dbus_bus_get_private(DBUS_BUS_SESSION, NULL);
    CHECK(c != NULL);
    if (!c)
        return 1;

    daemon = spawn(argv[1]);
    CHECK(daemon > 0);
    CHECK(wait_for_name(c));

    check_server_information(c);
    check_capabilities(c);
    check_introspection(c);
    check_notifications(c);

    second = spawn(argv[1]);
    CHECK(waitpid(second, &status, 0) == second);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) != 0);

    kill(daemon, SIGTERM);
    CHECK(waitpid(daemon, &status, 0) == daemon);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);

    dbus_connection_close(c);
    dbus_connection_unref(c);
    return check_failures ? 1 : 0;
}
