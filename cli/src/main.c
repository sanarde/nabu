/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "client.h"

static const char usage[] =
    "usage: nabu list\n"
    "       nabu history\n"
    "       nabu clear\n";

static const char *const urgency_names[] = {"low", "normal", "critical"};
static const char *const reason_names[] = {"unknown", "expired", "dismissed", "closed",
                                           "undefined"};

static int usage_error(void)
{
    fputs(usage, stderr);
    return 2;
}

static void print_field(const char *text)
{
    for (; *text; text++)
        putchar(*text == '\t' || *text == '\n' || *text == '\r' ? ' ' : *text);
}

static const char *urgency_name(unsigned char urgency)
{
    return urgency < sizeof(urgency_names) / sizeof(urgency_names[0])
               ? urgency_names[urgency]
               : "unknown";
}

static const char *reason_name(unsigned char reason)
{
    return reason < sizeof(reason_names) / sizeof(reason_names[0]) ? reason_names[reason]
                                                                    : "unknown";
}

static void skip(DBusMessageIter *iter, int count)
{
    for (int i = 0; i < count; i++)
        dbus_message_iter_next(iter);
}

static const char *next_string(DBusMessageIter *iter)
{
    const char *text = "";

    if (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_STRING)
        dbus_message_iter_get_basic(iter, &text);
    dbus_message_iter_next(iter);
    return text;
}

static int cmd_list(struct nabu_client *client)
{
    DBusMessage *reply = nabu_client_call(client, nabu_client_method("GetActive"));
    DBusMessageIter iter, array;

    if (!reply)
        return 1;
    dbus_message_iter_init(reply, &iter);
    dbus_message_iter_recurse(&iter, &array);
    while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_STRUCT) {
        DBusMessageIter fields;
        dbus_uint32_t id;
        unsigned char urgency;
        const char *app, *summary;

        dbus_message_iter_recurse(&array, &fields);
        dbus_message_iter_get_basic(&fields, &id);
        dbus_message_iter_next(&fields);
        app = next_string(&fields);
        skip(&fields, 1);
        summary = next_string(&fields);
        skip(&fields, 4);
        dbus_message_iter_get_basic(&fields, &urgency);

        printf("%u\t%s\t", id, urgency_name(urgency));
        print_field(app);
        putchar('\t');
        print_field(summary);
        putchar('\n');
        dbus_message_iter_next(&array);
    }
    dbus_message_unref(reply);
    return 0;
}

static int cmd_history(struct nabu_client *client)
{
    DBusMessage *reply = nabu_client_call(client, nabu_client_method("GetHistory"));
    DBusMessageIter iter, array;

    if (!reply)
        return 1;
    dbus_message_iter_init(reply, &iter);
    dbus_message_iter_recurse(&iter, &array);
    while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_STRUCT) {
        DBusMessageIter fields;
        dbus_int64_t when;
        unsigned char urgency, reason;
        const char *app, *summary;
        char stamp[32];
        time_t seconds;
        struct tm local;

        dbus_message_iter_recurse(&array, &fields);
        dbus_message_iter_get_basic(&fields, &when);
        dbus_message_iter_next(&fields);
        dbus_message_iter_get_basic(&fields, &urgency);
        dbus_message_iter_next(&fields);
        dbus_message_iter_get_basic(&fields, &reason);
        dbus_message_iter_next(&fields);
        app = next_string(&fields);
        skip(&fields, 1);
        summary = next_string(&fields);

        seconds = (time_t)when;
        localtime_r(&seconds, &local);
        strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &local);
        printf("%s\t%s\t%s\t", stamp, reason_name(reason), urgency_name(urgency));
        print_field(app);
        putchar('\t');
        print_field(summary);
        putchar('\n');
        dbus_message_iter_next(&array);
    }
    dbus_message_unref(reply);
    return 0;
}

static int cmd_clear(struct nabu_client *client)
{
    DBusMessage *reply = nabu_client_call(client, nabu_client_method("ClearHistory"));

    if (!reply)
        return 1;
    dbus_message_unref(reply);
    return 0;
}

int main(int argc, char **argv)
{
    struct nabu_client client;
    int rc;

    if (argc != 2)
        return usage_error();
    if (strcmp(argv[1], "list") != 0 && strcmp(argv[1], "history") != 0 &&
        strcmp(argv[1], "clear") != 0)
        return usage_error();
    if (nabu_client_open(&client) != 0)
        return 1;

    if (strcmp(argv[1], "list") == 0)
        rc = cmd_list(&client);
    else if (strcmp(argv[1], "history") == 0)
        rc = cmd_history(&client);
    else
        rc = cmd_clear(&client);

    fflush(stdout);
    nabu_client_close(&client);
    return rc;
}
