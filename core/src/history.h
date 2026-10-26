/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NABU_HISTORY_H
#define NABU_HISTORY_H

#include <stddef.h>
#include <stdint.h>

#include "notes.h"

struct nabu_history_entry {
    int64_t time;
    uint8_t urgency;
    uint8_t reason;
    char *app_name;
    char *app_icon;
    char *summary;
    char *body;
    char *desktop_entry;
    char *image_path;
};

/* Entries are ordered from the oldest to the newest. */
struct nabu_history {
    struct nabu_history_entry *items;
    size_t count;
    size_t capacity;
    size_t limit;
    char *path;
};

/*
 * Loads the file at path when it exists and keeps at most limit entries.
 * Lines that cannot be parsed are skipped. A NULL path keeps the history in
 * memory only. Returns -1 when memory is exhausted.
 */
int nabu_history_open(struct nabu_history *history, const char *path, size_t limit);
void nabu_history_close(struct nabu_history *history);

/*
 * Appends a closed notification, drops the oldest entries over the limit and
 * saves the file. Returns -1 when memory is exhausted and -2 when the entry
 * was kept in memory but the file could not be written.
 */
int nabu_history_add(struct nabu_history *history, const struct nabu_note *note,
                     uint8_t reason, int64_t time);

/* Removes all entries and saves the file. Returns -2 when saving failed. */
int nabu_history_clear(struct nabu_history *history);

#endif
