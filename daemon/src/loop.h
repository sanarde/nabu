/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NABUD_LOOP_H
#define NABUD_LOOP_H

enum nabud_source_kind {
    NABUD_SRC_SIGNAL,
    NABUD_SRC_TIMER,
    NABUD_SRC_CONFIG,
    NABUD_SRC_BUS,
};

struct nabud_source {
    enum nabud_source_kind kind;
};

#endif
