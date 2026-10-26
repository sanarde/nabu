/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NABUD_CONTROL_H
#define NABUD_CONTROL_H

#include <stdint.h>

struct nabud_bus;

int nabud_control_open(struct nabud_bus *bus);
void nabud_control_posted(struct nabud_bus *bus, uint32_t id);
void nabud_control_closed(struct nabud_bus *bus, uint32_t id, uint32_t reason);

#endif
