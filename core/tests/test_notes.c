/* SPDX-License-Identifier: GPL-3.0-only */
#include "check.h"
#include "notes.h"

static const struct nabu_note_input base = {
    .app_name = "app",
    .app_icon = "",
    .summary = "summary",
    .body = "body",
    .urgency = NABU_URGENCY_NORMAL,
    .timeout_ms = -1,
};

static void test_post_and_find(void)
{
    struct nabu_notes notes;
    struct nabu_note_input input = base;
    uint32_t a, b;

    nabu_notes_init(&notes);
    a = nabu_notes_post(&notes, 0, &input, 1000);
    input.summary = "second";
    b = nabu_notes_post(&notes, 0, &input, 1000);
    CHECK(a == 1 && b == 2);
    CHECK(notes.count == 2);
    CHECK_STR(nabu_notes_find(&notes, a)->summary, "summary");
    CHECK_STR(nabu_notes_find(&notes, b)->summary, "second");
    CHECK(nabu_notes_find(&notes, 99) == NULL);
    nabu_notes_free(&notes);
}

static void test_replace(void)
{
    struct nabu_notes notes;
    struct nabu_note_input input = base;
    uint32_t a, again, other;

    nabu_notes_init(&notes);
    a = nabu_notes_post(&notes, 0, &input, 1000);
    input.summary = "changed";
    again = nabu_notes_post(&notes, a, &input, 2000);
    CHECK(again == a);
    CHECK(notes.count == 1);
    CHECK_STR(nabu_notes_find(&notes, a)->summary, "changed");
    CHECK(nabu_notes_find(&notes, a)->expires_ms == 2000 + NABU_DEFAULT_TIMEOUT_MS);

    other = nabu_notes_post(&notes, 77, &input, 2000);
    CHECK(other != 77 && other != a);
    CHECK(notes.count == 2);
    nabu_notes_free(&notes);
}

static void test_timeouts(void)
{
    struct nabu_notes notes;
    struct nabu_note_input input = base;
    uint32_t forever, shorter, longer;

    nabu_notes_init(&notes);
    input.timeout_ms = 0;
    forever = nabu_notes_post(&notes, 0, &input, 1000);
    CHECK(nabu_notes_find(&notes, forever)->expires_ms == -1);
    CHECK(nabu_notes_next_expiry(&notes) == -1);

    input.timeout_ms = 3000;
    longer = nabu_notes_post(&notes, 0, &input, 1000);
    input.timeout_ms = 500;
    shorter = nabu_notes_post(&notes, 0, &input, 1000);
    CHECK(nabu_notes_next_expiry(&notes) == 1500);

    CHECK(nabu_notes_next_expired(&notes, 1499) == 0);
    CHECK(nabu_notes_next_expired(&notes, 1500) == shorter);
    CHECK(nabu_notes_close(&notes, shorter));
    CHECK(nabu_notes_next_expiry(&notes) == 4000);
    CHECK(nabu_notes_next_expired(&notes, 9000) == longer);
    CHECK(nabu_notes_close(&notes, longer));
    CHECK(nabu_notes_next_expired(&notes, 9000) == 0);
    CHECK(nabu_notes_find(&notes, forever) != NULL);
    nabu_notes_free(&notes);
}

static void test_close(void)
{
    struct nabu_notes notes;
    uint32_t a, b, c;

    nabu_notes_init(&notes);
    a = nabu_notes_post(&notes, 0, &base, 0);
    b = nabu_notes_post(&notes, 0, &base, 0);
    c = nabu_notes_post(&notes, 0, &base, 0);
    CHECK(nabu_notes_close(&notes, b));
    CHECK(!nabu_notes_close(&notes, b));
    CHECK(notes.count == 2);
    CHECK(nabu_notes_find(&notes, a) && nabu_notes_find(&notes, c));
    nabu_notes_free(&notes);
}

static void test_critical_never_expires(void)
{
    struct nabu_notes notes;
    struct nabu_note_input input = base;

    nabu_notes_init(&notes);
    input.urgency = NABU_URGENCY_CRITICAL;
    input.timeout_ms = 100;
    CHECK(nabu_notes_post(&notes, 0, &input, 0) != 0);
    CHECK(nabu_notes_next_expiry(&notes) == -1);
    CHECK(nabu_notes_next_expired(&notes, 100000) == 0);
    CHECK(notes.count == 1);
    nabu_notes_free(&notes);
}

static void test_hints_and_actions(void)
{
    static const char *const actions[] = {"default", "Open", "mute", "Mute", "stray"};
    struct nabu_notes notes;
    struct nabu_note_input input = base;
    const struct nabu_note *note;
    uint32_t id;

    nabu_notes_init(&notes);
    input.actions = actions;
    input.action_count = 5;
    input.desktop_entry = "org.example.app";
    input.image_path = "/tmp/image.png";
    input.urgency = NABU_URGENCY_LOW;
    input.transient = true;
    input.resident = true;
    id = nabu_notes_post(&notes, 0, &input, 0);
    note = nabu_notes_find(&notes, id);
    CHECK(note != NULL);
    CHECK(note->action_count == 4);
    CHECK_STR(note->actions[0], "default");
    CHECK_STR(note->actions[1], "Open");
    CHECK_STR(note->actions[2], "mute");
    CHECK_STR(note->actions[3], "Mute");
    CHECK_STR(note->desktop_entry, "org.example.app");
    CHECK_STR(note->image_path, "/tmp/image.png");
    CHECK(note->urgency == NABU_URGENCY_LOW);
    CHECK(note->transient && note->resident);

    CHECK(nabu_notes_post(&notes, id, &base, 0) == id);
    note = nabu_notes_find(&notes, id);
    CHECK(note->action_count == 0 && note->actions == NULL);
    CHECK_STR(note->desktop_entry, "");
    CHECK(note->urgency == NABU_URGENCY_NORMAL);
    CHECK(!note->transient && !note->resident);
    nabu_notes_free(&notes);
}

static void test_many(void)
{
    struct nabu_notes notes;

    nabu_notes_init(&notes);
    for (int i = 0; i < 100; i++)
        CHECK(nabu_notes_post(&notes, 0, &base, 0) == (uint32_t)i + 1);
    CHECK(notes.count == 100);
    nabu_notes_free(&notes);
}

int main(void)
{
    test_post_and_find();
    test_replace();
    test_timeouts();
    test_close();
    test_critical_never_expires();
    test_hints_and_actions();
    test_many();
    return check_failures ? 1 : 0;
}
