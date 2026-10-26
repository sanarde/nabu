/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NABUD_CONTROL_H
#define NABUD_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

struct nabud_bus;

int nabud_control_open(struct nabud_bus *bus);
void nabud_control_posted(struct nabud_bus *bus, uint32_t id);
void nabud_control_closed(struct nabud_bus *bus, uint32_t id, uint32_t reason);

/* Sets the mode and emits PropertiesChanged. Returns false when it was already set. */
bool nabud_control_set_do_not_disturb(struct nabud_bus *bus, bool enabled);

#endif
