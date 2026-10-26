/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "check.h"
#include "history.h"

static char root[] = "/tmp/nabu-history-XXXXXX";
static char path[128];

static struct nabu_note make_note(char *summary, char *body)
{
    struct nabu_note note = {0};

    note.app_name = "app";
    note.app_icon = "icon";
    note.summary = summary;
    note.body = body;
    note.desktop_entry = "org.example.app";
    note.image_path = "";
    note.urgency = NABU_URGENCY_NORMAL;
    return note;
}

static void test_memory_only(void)
{
    struct nabu_history history;
    struct nabu_note note = make_note("one", "body");

    CHECK(nabu_history_open(&history, NULL, 3) == 0);
    CHECK(nabu_history_add(&history, &note, NABU_CLOSE_EXPIRED, 10) == 0);
    CHECK(history.count == 1);
    CHECK_STR(history.items[0].summary, "one");
    CHECK(history.items[0].reason == NABU_CLOSE_EXPIRED);
    CHECK(history.items[0].time == 10);
    nabu_history_close(&history);
}

static void test_limit(void)
{
    struct nabu_history history;
    char summary[16];

    CHECK(nabu_history_open(&history, NULL, 3) == 0);
    for (int i = 0; i < 5; i++) {
        struct nabu_note note;

        snprintf(summary, sizeof(summary), "n%d", i);
        note = make_note(summary, "");
        CHECK(nabu_history_add(&history, &note, NABU_CLOSE_DISMISSED, i) == 0);
    }
    CHECK(history.count == 3);
    CHECK_STR(history.items[0].summary, "n2");
    CHECK_STR(history.items[2].summary, "n4");
    nabu_history_close(&history);
}

static void test_persistence(void)
{
    struct nabu_history history;
    struct nabu_note plain = make_note("plain", "text");
    struct nabu_note odd = make_note("tab\there", "line one\nline two \\ end\r");
    struct stat st;

    CHECK(nabu_history_open(&history, path, 10) == 0);
    CHECK(nabu_history_add(&history, &plain, NABU_CLOSE_EXPIRED, 100) == 0);
    odd.urgency = NABU_URGENCY_CRITICAL;
    CHECK(nabu_history_add(&history, &odd, NABU_CLOSE_CALLED, 200) == 0);
    nabu_history_close(&history);

    CHECK(stat(path, &st) == 0);
    CHECK((st.st_mode & 0777) == 0600);

    CHECK(nabu_history_open(&history, path, 10) == 0);
    CHECK(history.count == 2);
    CHECK_STR(history.items[0].summary, "plain");
    CHECK(history.items[0].time == 100);
    CHECK_STR(history.items[1].summary, "tab\there");
    CHECK_STR(history.items[1].body, "line one\nline two \\ end\r");
    CHECK_STR(history.items[1].desktop_entry, "org.example.app");
    CHECK(history.items[1].urgency == NABU_URGENCY_CRITICAL);
    CHECK(history.items[1].reason == NABU_CLOSE_CALLED);
    nabu_history_close(&history);

    CHECK(nabu_history_open(&history, path, 1) == 0);
    CHECK(history.count == 1);
    CHECK_STR(history.items[0].summary, "tab\there");
    nabu_history_close(&history);
}

static void test_clear(void)
{
    struct nabu_history history;
    struct nabu_note note = make_note("gone", "");

    CHECK(nabu_history_open(&history, path, 10) == 0);
    CHECK(nabu_history_add(&history, &note, NABU_CLOSE_EXPIRED, 1) == 0);
    CHECK(nabu_history_clear(&history) == 0);
    CHECK(history.count == 0);
    nabu_history_close(&history);

    CHECK(nabu_history_open(&history, path, 10) == 0);
    CHECK(history.count == 0);
    nabu_history_close(&history);
}

static void test_corrupt(void)
{
    struct nabu_history history;
    FILE *file = fopen(path, "w");

    CHECK(file != NULL);
    fputs("nabu-history 1\n"
          "garbage line\n"
          "5\t9\t1\ta\ti\ts\tb\td\tp\n"
          "7\t1\t1\ta\ti\tkept\tb\td\tp\n"
          "8\t1\n",
          file);
    fclose(file);

    CHECK(nabu_history_open(&history, path, 10) == 0);
    CHECK(history.count == 1);
    CHECK_STR(history.items[0].summary, "kept");
    nabu_history_close(&history);

    file = fopen(path, "w");
    fputs("something else\n7\t1\t1\ta\ti\ts\tb\td\tp\n", file);
    fclose(file);
    CHECK(nabu_history_open(&history, path, 10) == 0);
    CHECK(history.count == 0);
    nabu_history_close(&history);
}

static void test_creates_directory(void)
{
    struct nabu_history history;
    char nested[160];
    struct nabu_note note = make_note("x", "");

    snprintf(nested, sizeof(nested), "%s/a/b/history", root);
    CHECK(nabu_history_open(&history, nested, 5) == 0);
    CHECK(nabu_history_add(&history, &note, NABU_CLOSE_EXPIRED, 1) == 0);
    nabu_history_close(&history);
    CHECK(access(nested, R_OK) == 0);
    unlink(nested);
    snprintf(nested, sizeof(nested), "%s/a/b", root);
    rmdir(nested);
    snprintf(nested, sizeof(nested), "%s/a", root);
    rmdir(nested);
}

int main(void)
{
    if (!mkdtemp(root))
        return 1;
    snprintf(path, sizeof(path), "%s/history", root);

    test_memory_only();
    test_limit();
    test_persistence();
    test_clear();
    test_corrupt();
    test_creates_directory();

    unlink(path);
    rmdir(root);
    return check_failures ? 1 : 0;
}
