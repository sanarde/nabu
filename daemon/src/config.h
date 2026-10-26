/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NABUD_CONFIG_H
#define NABUD_CONFIG_H

#include <cfgd/cfgd.h>

#include "loop.h"

struct nabud_bus;

struct nabud_config {
    struct nabud_source source;
    struct nabud_bus *bus;
    cfgd_t *cfgd;
};

/*
 * Applies the settings from cfgd to the bus and keeps them applied while they
 * change. The descriptor of cfgd is added to the epoll set epfd. Returns -1
 * when cfgd cannot be used, in which case the built-in defaults stay.
 */
int nabud_config_open(struct nabud_config *config, struct nabud_bus *bus, int epfd);
void nabud_config_close(struct nabud_config *config);
void nabud_config_dispatch(struct nabud_config *config);

#endif
