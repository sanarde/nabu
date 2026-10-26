/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NABU_NOTES_H
#define NABU_NOTES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NABU_DEFAULT_TIMEOUT_MS 5000

enum nabu_close_reason {
    NABU_CLOSE_EXPIRED = 1,
    NABU_CLOSE_DISMISSED = 2,
    NABU_CLOSE_CALLED = 3,
    NABU_CLOSE_UNDEFINED = 4,
};

enum nabu_urgency {
    NABU_URGENCY_LOW = 0,
    NABU_URGENCY_NORMAL = 1,
    NABU_URGENCY_CRITICAL = 2,
};

struct nabu_note {
    uint32_t id;
    char *app_name;
    char *app_icon;
    char *summary;
    char *body;
    char *desktop_entry;
    char *image_path;
    char **actions;
    size_t action_count;
    uint8_t urgency;
    bool transient;
    bool resident;
    int64_t expires_ms;
};

/*
 * Optional strings may be NULL. actions holds key and label pairs, so an
 * unpaired last string is dropped. urgency must be set explicitly, as the
 * zero value is NABU_URGENCY_LOW.
 */
struct nabu_note_input {
    const char *app_name;
    const char *app_icon;
    const char *summary;
    const char *body;
    const char *desktop_entry;
    const char *image_path;
    const char *const *actions;
    size_t action_count;
    uint8_t urgency;
    bool transient;
    bool resident;
    int32_t timeout_ms;
};

struct nabu_notes {
    struct nabu_note *items;
    size_t count;
    size_t capacity;
    uint32_t last_id;
};

int64_t nabu_now_ms(void);

void nabu_notes_init(struct nabu_notes *notes);
void nabu_notes_free(struct nabu_notes *notes);

/*
 * Adds a notification, or replaces the one with replaces_id in place. A
 * timeout of -1 selects the default and 0 means no expiry. Critical
 * notifications never expire. Returns the id, or 0 when memory is exhausted.
 */
uint32_t nabu_notes_post(struct nabu_notes *notes, uint32_t replaces_id,
                         const struct nabu_note_input *input, int64_t now_ms);

const struct nabu_note *nabu_notes_find(const struct nabu_notes *notes, uint32_t id);
bool nabu_notes_close(struct nabu_notes *notes, uint32_t id);

/* Returns the earliest expiry time in milliseconds, or -1 when none is set. */
int64_t nabu_notes_next_expiry(const struct nabu_notes *notes);

/* Returns the id of a notification that has expired at now_ms, or 0 when none has. */
uint32_t nabu_notes_next_expired(const struct nabu_notes *notes, int64_t now_ms);

#endif
