/* SPDX-License-Identifier: GPL-3.0-only */
#include <dbus/dbus.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "check.h"

#define NAME "org.freedesktop.Notifications"
#define PATH "/org/freedesktop/Notifications"
#define IFACE "org.freedesktop.Notifications"

#define CTL_NAME "org.nuros.sanar.nabu"
#define CTL_PATH "/org/nuros/sanar/nabu"
#define CTL_IFACE "org.nuros.sanar.Nabu"

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
        if (dbus_bus_name_has_owner(c, NAME, NULL) &&
            dbus_bus_name_has_owner(c, CTL_NAME, NULL))
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

static char state_dir[] = "/tmp/nabu-state-XXXXXX";
static char history_file[160];
static dbus_bool_t hint_transient = TRUE;

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

static uint32_t notify_urgency(DBusConnection *c, uint32_t replaces_id,
                               const char *summary, int32_t timeout_ms, int urgency)
{
    DBusMessage *message = dbus_message_new_method_call(NAME, PATH, IFACE, "Notify");
    DBusMessageIter iter, array;
    DBusMessage *reply;
    const char *app = "test", *icon = "", *body = "body";
    const char *action_key = "default", *action_label = "Open";
    const char *entry = "org.example.app", *wrong = "not a boolean";
    dbus_bool_t yes = hint_transient;
    unsigned char level = (unsigned char)urgency;
    uint32_t id = 0;

    dbus_message_append_args(message, DBUS_TYPE_STRING, &app, DBUS_TYPE_UINT32,
                             &replaces_id, DBUS_TYPE_STRING, &icon, DBUS_TYPE_STRING,
                             &summary, DBUS_TYPE_STRING, &body, DBUS_TYPE_INVALID);
    dbus_message_iter_init_append(message, &iter);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "s", &array);
    dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &action_key);
    dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &action_label);
    dbus_message_iter_close_container(&iter, &array);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &array);
    if (urgency >= 0)
        append_hint(&array, "urgency", DBUS_TYPE_BYTE, "y", &level);
    append_hint(&array, "transient", DBUS_TYPE_BOOLEAN, "b", &yes);
    append_hint(&array, "resident", DBUS_TYPE_STRING, "s", &wrong);
    append_hint(&array, "desktop-entry", DBUS_TYPE_STRING, "s", &entry);
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

static uint32_t notify(DBusConnection *c, uint32_t replaces_id, const char *summary,
                       int32_t timeout_ms)
{
    return notify_urgency(c, replaces_id, summary, timeout_ms, -1);
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

static int wait_signal(DBusConnection *c, const char *iface, const char *member,
                       uint32_t id, uint32_t *reason, int timeout_ms)
{
    for (int waited = 0; waited <= timeout_ms; waited += 50) {
        DBusMessage *message;

        dbus_connection_read_write(c, 50);
        while ((message = dbus_connection_pop_message(c))) {
            uint32_t got_id, got_reason;

            if (dbus_message_is_signal(message, iface, member) &&
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

static int wait_closed(DBusConnection *c, uint32_t id, uint32_t *reason, int timeout_ms)
{
    return wait_signal(c, IFACE, "NotificationClosed", id, reason, timeout_ms);
}

static int wait_posted(DBusConnection *c, uint32_t id, int timeout_ms)
{
    for (int waited = 0; waited <= timeout_ms; waited += 50) {
        DBusMessage *message;

        dbus_connection_read_write(c, 50);
        while ((message = dbus_connection_pop_message(c))) {
            DBusMessageIter iter, fields;
            uint32_t got_id = 0;

            if (dbus_message_is_signal(message, CTL_IFACE, "Posted") &&
                dbus_message_iter_init(message, &iter) &&
                dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRUCT) {
                dbus_message_iter_recurse(&iter, &fields);
                dbus_message_iter_get_basic(&fields, &got_id);
                if (got_id == id) {
                    dbus_message_unref(message);
                    return 1;
                }
            }
            dbus_message_unref(message);
        }
    }
    return 0;
}

static DBusMessage *ctl_call(DBusConnection *c, const char *method, int has_id,
                             uint32_t id)
{
    DBusMessage *message = dbus_message_new_method_call(CTL_NAME, CTL_PATH, CTL_IFACE, method);
    DBusMessage *reply;

    if (has_id)
        dbus_message_append_args(message, DBUS_TYPE_UINT32, &id, DBUS_TYPE_INVALID);
    reply = dbus_connection_send_with_reply_and_block(c, message, 5000, NULL);
    dbus_message_unref(message);
    CHECK(reply != NULL);
    return reply;
}

static size_t count_items(DBusMessage *reply)
{
    DBusMessageIter iter, array;
    size_t count = 0;

    if (!reply || !dbus_message_iter_init(reply, &iter))
        return 0;
    dbus_message_iter_recurse(&iter, &array);
    while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_STRUCT) {
        count++;
        dbus_message_iter_next(&array);
    }
    return count;
}

static int has_summary(DBusMessage *reply, int index, const char *summary)
{
    DBusMessageIter iter, array;

    if (!reply || !dbus_message_iter_init(reply, &iter))
        return 0;
    dbus_message_iter_recurse(&iter, &array);
    while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_STRUCT) {
        DBusMessageIter fields;
        const char *text = NULL;

        dbus_message_iter_recurse(&array, &fields);
        for (int i = 0; i < index; i++)
            dbus_message_iter_next(&fields);
        if (dbus_message_iter_get_arg_type(&fields) == DBUS_TYPE_STRING)
            dbus_message_iter_get_basic(&fields, &text);
        if (text && strcmp(text, summary) == 0)
            return 1;
        dbus_message_iter_next(&array);
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

    hint_transient = FALSE;
    b = notify(c, 0, "short", 100);
    CHECK(wait_closed(c, b, &reason, 2000));
    CHECK(reason == 1);

    b = notify_urgency(c, 0, "critical", 100, 2);
    CHECK(b != 0);
    CHECK(!wait_closed(c, b, &reason, 500));
    close_notification(c, b);
    CHECK(wait_closed(c, b, &reason, 2000));
    CHECK(reason == 3);

    hint_transient = TRUE;
    b = notify_urgency(c, 0, "low", 100, 0);
    CHECK(wait_closed(c, b, &reason, 2000));
    CHECK(reason == 1);
}

static void check_history(void)
{
    char text[4096] = "";
    FILE *file = fopen(history_file, "r");
    size_t length;

    CHECK(file != NULL);
    if (!file)
        return;
    length = fread(text, 1, sizeof(text) - 1, file);
    text[length] = '\0';
    fclose(file);
    CHECK(strstr(text, "\tshort\t") != NULL);
    CHECK(strstr(text, "\tcritical\t") != NULL);
    CHECK(strstr(text, "first again") == NULL);
    CHECK(strstr(text, "\tlow\t") == NULL);
}

static void check_control(DBusConnection *c)
{
    DBusMessage *reply;
    uint32_t a, b, reason = 0;
    size_t before;

    dbus_bus_add_match(c, "type='signal',interface='" CTL_IFACE "'", NULL);
    hint_transient = FALSE;
    a = notify(c, 0, "ctl-a", 0);
    b = notify(c, 0, "ctl-b", 0);
    CHECK(wait_posted(c, a, 2000));
    CHECK(wait_posted(c, b, 2000));

    reply = ctl_call(c, "GetActive", 0, 0);
    CHECK(has_summary(reply, 3, "ctl-a") && has_summary(reply, 3, "ctl-b"));
    before = count_items(reply);
    dbus_message_unref(reply);

    dbus_message_unref(ctl_call(c, "Dismiss", 1, a));
    CHECK(wait_signal(c, CTL_IFACE, "Closed", a, &reason, 2000));
    CHECK(reason == 2);
    dbus_message_unref(ctl_call(c, "Dismiss", 1, 9999));

    reply = ctl_call(c, "GetActive", 0, 0);
    CHECK(!has_summary(reply, 3, "ctl-a") && has_summary(reply, 3, "ctl-b"));
    CHECK(count_items(reply) == before - 1);
    dbus_message_unref(reply);

    reply = ctl_call(c, "GetHistory", 0, 0);
    CHECK(has_summary(reply, 5, "ctl-a"));
    CHECK(!has_summary(reply, 5, "ctl-b"));
    dbus_message_unref(reply);

    dbus_message_unref(ctl_call(c, "ClearHistory", 0, 0));
    reply = ctl_call(c, "GetHistory", 0, 0);
    CHECK(count_items(reply) == 0);
    dbus_message_unref(reply);

    close_notification(c, b);
    hint_transient = TRUE;
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

    if (argc != 2 || !mkdtemp(state_dir))
        return 1;
    setenv("XDG_STATE_HOME", state_dir, 1);
    snprintf(history_file, sizeof(history_file), "%s/nabu/history", state_dir);
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
    check_history();
    check_control(c);

    second = spawn(argv[1]);
    CHECK(waitpid(second, &status, 0) == second);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) != 0);

    kill(daemon, SIGTERM);
    CHECK(waitpid(daemon, &status, 0) == daemon);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);

    dbus_connection_close(c);
    dbus_connection_unref(c);
    unlink(history_file);
    snprintf(history_file, sizeof(history_file), "%s/nabu", state_dir);
    rmdir(history_file);
    rmdir(state_dir);
    return check_failures ? 1 : 0;
}
