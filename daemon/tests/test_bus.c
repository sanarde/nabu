/* SPDX-License-Identifier: GPL-3.0-only */
#include <dbus/dbus.h>
#include <signal.h>
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
