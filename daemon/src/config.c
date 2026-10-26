/* SPDX-License-Identifier: GPL-3.0-only */
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>

#include "bus.h"

#define PREFIX "/org/nuros/sanar/nabu/"
#define KEY_TIMEOUT PREFIX "default-timeout"
#define KEY_HISTORY_LIMIT PREFIX "history-limit"

static void apply(struct nabud_config *config, const char *key)
{
    struct nabud_bus *bus = config->bus;
    int64_t value;

    if (strcmp(key, KEY_TIMEOUT) == 0) {
        if (cfgd_get_int(config->cfgd, key, &value))
            bus->notes.default_timeout_ms = (int32_t)value;
    } else if (strcmp(key, KEY_HISTORY_LIMIT) == 0) {
        if (cfgd_get_int(config->cfgd, key, &value) &&
            nabu_history_set_limit(&bus->history, (size_t)value) != 0)
            fprintf(stderr, "nabud: cannot save history\n");
    }
}

static void changed(const char *key, void *user)
{
    apply(user, key);
}

int nabud_config_open(struct nabud_config *config, struct nabud_bus *bus, int epfd)
{
    struct epoll_event ev = {.events = EPOLLIN, .data.ptr = &config->source};
    int fd;

    config->source.kind = NABUD_SRC_CONFIG;
    config->bus = bus;
    config->cfgd = cfgd_open();
    if (!config->cfgd)
        return -1;
    fd = cfgd_notify_fd(config->cfgd);
    if (fd < 0 || cfgd_subscribe(config->cfgd, PREFIX, changed, config) < 0 ||
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) != 0) {
        nabud_config_close(config);
        return -1;
    }
    apply(config, KEY_TIMEOUT);
    apply(config, KEY_HISTORY_LIMIT);
    return 0;
}

void nabud_config_close(struct nabud_config *config)
{
    cfgd_close(config->cfgd);
    config->cfgd = NULL;
}

void nabud_config_dispatch(struct nabud_config *config)
{
    cfgd_dispatch(config->cfgd);
}
