/* SPDX-License-Identifier: GPL-3.0-only */
#include "notes.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

int64_t nabu_now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void nabu_notes_init(struct nabu_notes *notes)
{
    memset(notes, 0, sizeof(*notes));
}

static void note_clear(struct nabu_note *note)
{
    free(note->app_name);
    free(note->app_icon);
    free(note->summary);
    free(note->body);
    memset(note, 0, sizeof(*note));
}

void nabu_notes_free(struct nabu_notes *notes)
{
    for (size_t i = 0; i < notes->count; i++)
        note_clear(&notes->items[i]);
    free(notes->items);
    nabu_notes_init(notes);
}

static int64_t expiry_for(int32_t timeout_ms, int64_t now_ms)
{
    if (timeout_ms == 0)
        return -1;
    if (timeout_ms < 0)
        timeout_ms = NABU_DEFAULT_TIMEOUT_MS;
    return now_ms + timeout_ms;
}

static int note_fill(struct nabu_note *note, const struct nabu_note_input *input,
                     int64_t now_ms)
{
    note->app_name = strdup(input->app_name);
    note->app_icon = strdup(input->app_icon);
    note->summary = strdup(input->summary);
    note->body = strdup(input->body);
    note->expires_ms = expiry_for(input->timeout_ms, now_ms);
    if (note->app_name && note->app_icon && note->summary && note->body)
        return 0;
    free(note->app_name);
    free(note->app_icon);
    free(note->summary);
    free(note->body);
    return -1;
}

static struct nabu_note *note_at(struct nabu_notes *notes, uint32_t id)
{
    for (size_t i = 0; i < notes->count; i++) {
        if (notes->items[i].id == id)
            return &notes->items[i];
    }
    return NULL;
}

const struct nabu_note *nabu_notes_find(const struct nabu_notes *notes, uint32_t id)
{
    return note_at((struct nabu_notes *)notes, id);
}

static uint32_t next_id(struct nabu_notes *notes)
{
    do {
        notes->last_id++;
    } while (notes->last_id == 0 || note_at(notes, notes->last_id));
    return notes->last_id;
}

uint32_t nabu_notes_post(struct nabu_notes *notes, uint32_t replaces_id,
                         const struct nabu_note_input *input, int64_t now_ms)
{
    struct nabu_note fresh = {0};
    struct nabu_note *existing = replaces_id ? note_at(notes, replaces_id) : NULL;

    if (note_fill(&fresh, input, now_ms) != 0)
        return 0;

    if (existing) {
        fresh.id = existing->id;
        note_clear(existing);
        *existing = fresh;
        return fresh.id;
    }

    if (notes->count == notes->capacity) {
        size_t capacity = notes->capacity ? notes->capacity * 2 : 8;
        struct nabu_note *items = realloc(notes->items, capacity * sizeof(*items));

        if (!items) {
            note_clear(&fresh);
            return 0;
        }
        notes->items = items;
        notes->capacity = capacity;
    }
    fresh.id = next_id(notes);
    notes->items[notes->count++] = fresh;
    return fresh.id;
}

static void remove_at(struct nabu_notes *notes, size_t index)
{
    note_clear(&notes->items[index]);
    memmove(&notes->items[index], &notes->items[index + 1],
            (notes->count - index - 1) * sizeof(notes->items[0]));
    notes->count--;
}

bool nabu_notes_close(struct nabu_notes *notes, uint32_t id)
{
    for (size_t i = 0; i < notes->count; i++) {
        if (notes->items[i].id == id) {
            remove_at(notes, i);
            return true;
        }
    }
    return false;
}

int64_t nabu_notes_next_expiry(const struct nabu_notes *notes)
{
    int64_t next = -1;

    for (size_t i = 0; i < notes->count; i++) {
        int64_t at = notes->items[i].expires_ms;

        if (at >= 0 && (next < 0 || at < next))
            next = at;
    }
    return next;
}

size_t nabu_notes_expire(struct nabu_notes *notes, int64_t now_ms, uint32_t *ids)
{
    size_t removed = 0;
    size_t i = 0;

    while (i < notes->count) {
        int64_t at = notes->items[i].expires_ms;

        if (at >= 0 && at <= now_ms) {
            ids[removed++] = notes->items[i].id;
            remove_at(notes, i);
        } else {
            i++;
        }
    }
    return removed;
}
