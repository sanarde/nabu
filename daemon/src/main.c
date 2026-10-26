/* SPDX-License-Identifier: GPL-3.0-only */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "bus.h"
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

int main(void)
{
    struct nabud_bus bus = {0};
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
    if (nabud_bus_open(&bus, epfd, timer_fd) != 0)
        return 1;

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
        nabud_bus_event(&bus, (struct nabud_bus_watch *)source, event.events);
        if (!bus.connection) {
            rc = 0;
            break;
        }
    }

    nabud_bus_close(&bus);
    return rc;
}
