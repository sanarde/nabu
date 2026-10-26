/* SPDX-License-Identifier: GPL-3.0-only */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
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

int main(void)
{
    struct nabud_bus bus = {0};
    struct nabud_source signal_source = {NABUD_SRC_SIGNAL};
    struct epoll_event ev = {.events = EPOLLIN, .data.ptr = &signal_source};
    int sig_fd, epfd, rc = 1;

    sig_fd = open_signal_fd();
    if (sig_fd < 0) {
        fprintf(stderr, "nabud: signalfd: %s\n", strerror(errno));
        return 1;
    }
    epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0 || epoll_ctl(epfd, EPOLL_CTL_ADD, sig_fd, &ev) != 0) {
        fprintf(stderr, "nabud: epoll: %s\n", strerror(errno));
        return 1;
    }
    if (nabud_bus_open(&bus, epfd) != 0)
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
        nabud_bus_event(&bus, (struct nabud_bus_watch *)source, event.events);
        if (!bus.connection) {
            rc = 0;
            break;
        }
    }

    nabud_bus_close(&bus);
    return rc;
}
