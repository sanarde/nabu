/* SPDX-License-Identifier: GPL-3.0-only */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "client.h"

static const char usage[] =
    "usage: nabu list\n"
    "       nabu history\n"
    "       nabu clear\n"
    "       nabu dismiss ID\n"
    "       nabu invoke ID KEY\n"
    "       nabu dnd [on|off|status]\n";

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

struct note_view {
    dbus_uint32_t id;
    unsigned char urgency;
    const char *app;
    const char *summary;
};

static void read_note(DBusMessageIter *structure, struct note_view *note)
{
    DBusMessageIter fields;

    dbus_message_iter_recurse(structure, &fields);
    dbus_message_iter_get_basic(&fields, &note->id);
    dbus_message_iter_next(&fields);
    note->app = next_string(&fields);
    skip(&fields, 1);
    note->summary = next_string(&fields);
    skip(&fields, 4);
    dbus_message_iter_get_basic(&fields, &note->urgency);
}

static void print_note(const struct note_view *note)
{
    printf("%u\t%s\t", note->id, urgency_name(note->urgency));
    print_field(note->app);
    putchar('\t');
    print_field(note->summary);
    putchar('\n');
}

static int cmd_list(struct nabu_client *client, int argc, char **argv)
{
    DBusMessage *reply = nabu_client_call(client, nabu_client_method("GetActive"));
    DBusMessageIter iter, array;

    (void)argc;
    (void)argv;
    if (!reply)
        return 1;
    dbus_message_iter_init(reply, &iter);
    dbus_message_iter_recurse(&iter, &array);
    while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_STRUCT) {
        struct note_view note;

        read_note(&array, &note);
        print_note(&note);
        dbus_message_iter_next(&array);
    }
    dbus_message_unref(reply);
    return 0;
}

static int cmd_history(struct nabu_client *client, int argc, char **argv)
{
    DBusMessage *reply = nabu_client_call(client, nabu_client_method("GetHistory"));
    DBusMessageIter iter, array;

    (void)argc;
    (void)argv;
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

static int cmd_clear(struct nabu_client *client, int argc, char **argv)
{
    DBusMessage *reply = nabu_client_call(client, nabu_client_method("ClearHistory"));

    (void)argc;
    (void)argv;
    if (!reply)
        return 1;
    dbus_message_unref(reply);
    return 0;
}

static int parse_id(const char *text, dbus_uint32_t *id)
{
    char *end;
    unsigned long value;

    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value == 0 || value > UINT32_MAX) {
        fprintf(stderr, "nabu: invalid id\n");
        return -1;
    }
    *id = (dbus_uint32_t)value;
    return 0;
}

static int cmd_dismiss(struct nabu_client *client, int argc, char **argv)
{
    DBusMessage *call, *reply;
    dbus_uint32_t id;

    (void)argc;
    if (parse_id(argv[0], &id) != 0)
        return 1;
    call = nabu_client_method("Dismiss");
    if (call)
        dbus_message_append_args(call, DBUS_TYPE_UINT32, &id, DBUS_TYPE_INVALID);
    reply = nabu_client_call(client, call);
    if (!reply)
        return 1;
    dbus_message_unref(reply);
    return 0;
}

static int cmd_invoke(struct nabu_client *client, int argc, char **argv)
{
    DBusMessage *call, *reply;
    dbus_uint32_t id;
    const char *key = argv[1];

    (void)argc;
    if (parse_id(argv[0], &id) != 0)
        return 1;
    call = nabu_client_method("Invoke");
    if (call)
        dbus_message_append_args(call, DBUS_TYPE_UINT32, &id, DBUS_TYPE_STRING, &key,
                                 DBUS_TYPE_INVALID);
    reply = nabu_client_call(client, call);
    if (!reply)
        return 1;
    dbus_message_unref(reply);
    return 0;
}

static int dnd_status(struct nabu_client *client)
{
    DBusMessage *call = nabu_client_properties_method("Get"), *reply;
    DBusMessageIter iter, variant;
    const char *interface = "org.nuros.sanar.Nabu", *name = "DoNotDisturb";
    dbus_bool_t enabled;

    if (call)
        dbus_message_append_args(call, DBUS_TYPE_STRING, &interface, DBUS_TYPE_STRING,
                                 &name, DBUS_TYPE_INVALID);
    reply = nabu_client_call(client, call);
    if (!reply)
        return 1;
    dbus_message_iter_init(reply, &iter);
    dbus_message_iter_recurse(&iter, &variant);
    dbus_message_iter_get_basic(&variant, &enabled);
    puts(enabled ? "on" : "off");
    dbus_message_unref(reply);
    return 0;
}

static int dnd_set(struct nabu_client *client, dbus_bool_t enabled)
{
    DBusMessage *call = nabu_client_properties_method("Set"), *reply;
    DBusMessageIter iter, variant;
    const char *interface = "org.nuros.sanar.Nabu", *name = "DoNotDisturb";

    if (call) {
        dbus_message_append_args(call, DBUS_TYPE_STRING, &interface, DBUS_TYPE_STRING,
                                 &name, DBUS_TYPE_INVALID);
        dbus_message_iter_init_append(call, &iter);
        dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "b", &variant);
        dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &enabled);
        dbus_message_iter_close_container(&iter, &variant);
    }
    reply = nabu_client_call(client, call);
    if (!reply)
        return 1;
    dbus_message_unref(reply);
    return 0;
}

static int cmd_dnd(struct nabu_client *client, int argc, char **argv)
{
    if (argc == 0 || strcmp(argv[0], "status") == 0)
        return dnd_status(client);
    if (strcmp(argv[0], "on") == 0)
        return dnd_set(client, TRUE);
    if (strcmp(argv[0], "off") == 0)
        return dnd_set(client, FALSE);
    return usage_error();
}

struct command {
    const char *name;
    int min_args;
    int max_args;
    int (*run)(struct nabu_client *client, int argc, char **argv);
};

static const struct command commands[] = {
    {"list", 0, 0, cmd_list},
    {"history", 0, 0, cmd_history},
    {"clear", 0, 0, cmd_clear},
    {"dismiss", 1, 1, cmd_dismiss},
    {"invoke", 2, 2, cmd_invoke},
    {"dnd", 0, 1, cmd_dnd},
};

int main(int argc, char **argv)
{
    const struct command *command = NULL;
    struct nabu_client client;
    int rc;

    if (argc < 2)
        return usage_error();
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        if (strcmp(argv[1], commands[i].name) == 0)
            command = &commands[i];
    }
    if (!command || argc - 2 < command->min_args || argc - 2 > command->max_args)
        return usage_error();
    if (nabu_client_open(&client) != 0)
        return 1;

    rc = command->run(&client, argc - 2, argv + 2);
    fflush(stdout);
    nabu_client_close(&client);
    return rc;
}
