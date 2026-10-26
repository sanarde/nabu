/* SPDX-License-Identifier: GPL-3.0-only */
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "bus.h"
#include "config.h"
#include "loop.h"

static int open_signal_fd(void)
{
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) != 0)
        return -1;
    return signalfd(-1, &mask, SFD_CLOEXEC);
}

static int add_epoll_source(int epfd, int fd, struct nabud_source *source)
{
    struct epoll_event ev = {.events = EPOLLIN, .data.ptr = source};

    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

static int history_path(char *buffer, size_t size)
{
    const char *state = getenv("XDG_STATE_HOME");
    const char *home = getenv("HOME");
    int n;

    if (state && state[0] == '/')
        n = snprintf(buffer, size, "%s/nabu/history", state);
    else if (home && home[0] == '/')
        n = snprintf(buffer, size, "%s/.local/state/nabu/history", home);
    else
        return -1;
    return n < (int)size ? 0 : -1;
}

int main(void)
{
    struct nabud_bus bus = {0};
    struct nabud_config config = {0};
    char path[PATH_MAX];
    struct nabud_source signal_source = {NABUD_SRC_SIGNAL};
    struct nabud_source timer_source = {NABUD_SRC_TIMER};
    int sig_fd, timer_fd, epfd, rc = 1;

    sig_fd = open_signal_fd();
    if (sig_fd < 0) {
        fprintf(stderr, "nabud: signalfd: %s\n", strerror(errno));
        return 1;
    }
    timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    epfd = epoll_create1(EPOLL_CLOEXEC);
    if (timer_fd < 0 || epfd < 0 || add_epoll_source(epfd, sig_fd, &signal_source) != 0 ||
        add_epoll_source(epfd, timer_fd, &timer_source) != 0) {
        fprintf(stderr, "nabud: epoll: %s\n", strerror(errno));
        return 1;
    }
    if (nabud_bus_open(&bus, epfd, timer_fd,
                       history_path(path, sizeof(path)) == 0 ? path : NULL) != 0)
        return 1;
    if (nabud_config_open(&config, &bus, epfd) != 0)
        fprintf(stderr, "nabud: cfgd is not available, using the default settings\n");

    for (;;) {
        struct epoll_event event;
        int n = epoll_wait(epfd, &event, 1, -1);
        struct nabud_source *source;

        if (n < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "nabud: epoll_wait: %s\n", strerror(errno));
            break;
        }
        source = event.data.ptr;

        if (source->kind == NABUD_SRC_SIGNAL) {
            rc = 0;
            break;
        }
        if (source->kind == NABUD_SRC_TIMER) {
            nabud_bus_expire(&bus);
            continue;
        }
        if (source->kind == NABUD_SRC_CONFIG) {
            nabud_config_dispatch(&config);
            continue;
        }
        nabud_bus_event(&bus, (struct nabud_bus_watch *)source, event.events);
        if (!bus.connection) {
            rc = 0;
            break;
        }
    }

    nabud_config_close(&config);
    nabud_bus_close(&bus);
    return rc;
}
