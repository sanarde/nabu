/* SPDX-License-Identifier: GPL-3.0-only */
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>

#include "bus.h"
#include "control.h"

#define PREFIX "/org/nuros/sanar/nabu/"
#define KEY_TIMEOUT PREFIX "default-timeout"
#define KEY_HISTORY_LIMIT PREFIX "history-limit"
#define KEY_DND PREFIX "do-not-disturb"

static void apply(struct nabud_config *config, const char *key)
{
    struct nabud_bus *bus = config->bus;
    int64_t value;
    bool enabled;

    if (strcmp(key, KEY_DND) == 0) {
        if (cfgd_get_bool(config->cfgd, key, &enabled))
            nabud_control_set_do_not_disturb(bus, enabled);
    } else if (strcmp(key, KEY_TIMEOUT) == 0) {
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
    bus->config = config;
    apply(config, KEY_TIMEOUT);
    apply(config, KEY_HISTORY_LIMIT);
    apply(config, KEY_DND);
    return 0;
}

void nabud_config_close(struct nabud_config *config)
{
    if (config->bus)
        config->bus->config = NULL;
    cfgd_close(config->cfgd);
    config->cfgd = NULL;
}

void nabud_config_set_do_not_disturb(struct nabud_config *config, bool enabled)
{
    if (cfgd_set_bool(config->cfgd, KEY_DND, enabled) != CFGD_OK)
        fprintf(stderr, "nabud: cannot save the do not disturb setting\n");
}

void nabud_config_dispatch(struct nabud_config *config)
{
    cfgd_dispatch(config->cfgd);
}
