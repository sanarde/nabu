/* SPDX-License-Identifier: GPL-3.0-only */
#include "history.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define HEADER "nabu-history 1"
#define FIELDS 9

static void entry_clear(struct nabu_history_entry *entry)
{
    free(entry->app_name);
    free(entry->app_icon);
    free(entry->summary);
    free(entry->body);
    free(entry->desktop_entry);
    free(entry->image_path);
    memset(entry, 0, sizeof(*entry));
}

static void drop_oldest(struct nabu_history *history, size_t drop)
{
    for (size_t i = 0; i < drop; i++)
        entry_clear(&history->items[i]);
    memmove(history->items, history->items + drop,
            (history->count - drop) * sizeof(*history->items));
    history->count -= drop;
}

static struct nabu_history_entry *append_slot(struct nabu_history *history)
{
    if (history->count == history->capacity) {
        size_t capacity = history->capacity ? history->capacity * 2 : 16;
        struct nabu_history_entry *items = realloc(history->items, capacity * sizeof(*items));

        if (!items)
            return NULL;
        history->items = items;
        history->capacity = capacity;
    }
    memset(&history->items[history->count], 0, sizeof(*history->items));
    return &history->items[history->count];
}

static int entry_fill(struct nabu_history_entry *entry, const char *const *text)
{
    entry->app_name = strdup(text[0]);
    entry->app_icon = strdup(text[1]);
    entry->summary = strdup(text[2]);
    entry->body = strdup(text[3]);
    entry->desktop_entry = strdup(text[4]);
    entry->image_path = strdup(text[5]);
    if (entry->app_name && entry->app_icon && entry->summary && entry->body &&
        entry->desktop_entry && entry->image_path)
        return 0;
    entry_clear(entry);
    return -1;
}

static void unescape(char *text)
{
    char *out = text;

    for (const char *in = text; *in; in++) {
        if (*in == '\\' && in[1]) {
            in++;
            *out++ = *in == 't' ? '\t' : *in == 'n' ? '\n' : *in == 'r' ? '\r' : *in;
        } else {
            *out++ = *in;
        }
    }
    *out = '\0';
}

static void write_escaped(FILE *file, const char *text)
{
    for (; *text; text++) {
        switch (*text) {
        case '\\':
            fputs("\\\\", file);
            break;
        case '\t':
            fputs("\\t", file);
            break;
        case '\n':
            fputs("\\n", file);
            break;
        case '\r':
            fputs("\\r", file);
            break;
        default:
            fputc(*text, file);
        }
    }
}

static int parse_number(const char *text, long long min, long long max, long long *out)
{
    char *end;

    errno = 0;
    *out = strtoll(text, &end, 10);
    return errno == 0 && end != text && *end == '\0' && *out >= min && *out <= max ? 0 : -1;
}

static int parse_line(struct nabu_history *history, char *line)
{
    char *field[FIELDS];
    struct nabu_history_entry *entry;
    long long time, urgency, reason;
    char *cursor = line;

    for (int i = 0; i < FIELDS; i++) {
        field[i] = cursor;
        if (i < FIELDS - 1) {
            cursor = strchr(cursor, '\t');
            if (!cursor)
                return 0;
            *cursor++ = '\0';
        }
    }
    if (parse_number(field[0], 0, INT64_MAX, &time) != 0 ||
        parse_number(field[1], 0, NABU_URGENCY_CRITICAL, &urgency) != 0 ||
        parse_number(field[2], 0, NABU_CLOSE_UNDEFINED, &reason) != 0)
        return 0;
    for (int i = 3; i < FIELDS; i++)
        unescape(field[i]);

    entry = append_slot(history);
    if (!entry || entry_fill(entry, (const char *const *)&field[3]) != 0)
        return -1;
    entry->time = time;
    entry->urgency = (uint8_t)urgency;
    entry->reason = (uint8_t)reason;
    history->count++;
    if (history->count > history->limit)
        drop_oldest(history, history->count - history->limit);
    return 0;
}

static int load(struct nabu_history *history)
{
    FILE *file = fopen(history->path, "r");
    char *line = NULL;
    size_t size = 0;
    ssize_t length;
    int rc = 0;

    if (!file)
        return 0;
    length = getline(&line, &size, file);
    if (length < 0 || strncmp(line, HEADER, sizeof(HEADER) - 1) != 0) {
        free(line);
        fclose(file);
        return 0;
    }
    while (rc == 0 && (length = getline(&line, &size, file)) >= 0) {
        if (length > 0 && line[length - 1] == '\n')
            line[length - 1] = '\0';
        rc = parse_line(history, line);
    }
    free(line);
    fclose(file);
    return rc;
}

int nabu_history_open(struct nabu_history *history, const char *path, size_t limit)
{
    memset(history, 0, sizeof(*history));
    history->limit = limit;
    if (!path)
        return 0;
    history->path = strdup(path);
    if (!history->path || load(history) != 0) {
        nabu_history_close(history);
        return -1;
    }
    return 0;
}

void nabu_history_close(struct nabu_history *history)
{
    for (size_t i = 0; i < history->count; i++)
        entry_clear(&history->items[i]);
    free(history->items);
    free(history->path);
    memset(history, 0, sizeof(*history));
}

static int make_parent(const char *path)
{
    char dir[PATH_MAX];
    char *slash;

    if (snprintf(dir, sizeof(dir), "%s", path) >= (int)sizeof(dir))
        return -1;
    slash = strrchr(dir, '/');
    if (!slash || slash == dir)
        return 0;
    for (char *p = dir + 1; p < slash; p++) {
        if (*p != '/')
            continue;
        *p = '\0';
        if (mkdir(dir, 0700) != 0 && errno != EEXIST)
            return -1;
        *p = '/';
    }
    *slash = '\0';
    return mkdir(dir, 0700) != 0 && errno != EEXIST ? -1 : 0;
}

static int save(const struct nabu_history *history)
{
    char temp[PATH_MAX];
    FILE *file;
    int failed;

    if (!history->path)
        return 0;
    if (snprintf(temp, sizeof(temp), "%s.tmp", history->path) >= (int)sizeof(temp) ||
        make_parent(history->path) != 0)
        return -1;
    file = fopen(temp, "w");
    if (!file)
        return -1;
    fchmod(fileno(file), 0600);
    fprintf(file, "%s\n", HEADER);
    for (size_t i = 0; i < history->count; i++) {
        const struct nabu_history_entry *e = &history->items[i];
        const char *const text[] = {e->app_name, e->app_icon, e->summary,
                                    e->body, e->desktop_entry, e->image_path};

        fprintf(file, "%lld\t%u\t%u", (long long)e->time, e->urgency, e->reason);
        for (size_t j = 0; j < sizeof(text) / sizeof(text[0]); j++) {
            fputc('\t', file);
            write_escaped(file, text[j]);
        }
        fputc('\n', file);
    }
    failed = fflush(file) != 0 || ferror(file) || fsync(fileno(file)) != 0;
    failed |= fclose(file) != 0;
    if (failed || rename(temp, history->path) != 0) {
        unlink(temp);
        return -1;
    }
    return 0;
}

int nabu_history_add(struct nabu_history *history, const struct nabu_note *note,
                     uint8_t reason, int64_t time)
{
    const char *const text[] = {note->app_name, note->app_icon, note->summary,
                                note->body, note->desktop_entry, note->image_path};
    struct nabu_history_entry *entry = append_slot(history);

    if (!entry || entry_fill(entry, text) != 0)
        return -1;
    entry->time = time;
    entry->urgency = note->urgency;
    entry->reason = reason;
    history->count++;
    if (history->count > history->limit)
        drop_oldest(history, history->count - history->limit);
    return save(history) == 0 ? 0 : -2;
}

int nabu_history_set_limit(struct nabu_history *history, size_t limit)
{
    history->limit = limit;
    if (history->count <= limit)
        return 0;
    drop_oldest(history, history->count - limit);
    return save(history) == 0 ? 0 : -2;
}

int nabu_history_clear(struct nabu_history *history)
{
    drop_oldest(history, history->count);
    return save(history) == 0 ? 0 : -2;
}
