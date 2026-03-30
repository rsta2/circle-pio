// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2024 Raspberry Pi Ltd.
 * All rights reserved.
 */
#ifndef _HARDWARE_PIO_H
#define _HARDWARE_PIO_H

#include "piolib.h"

static inline bool pio_claim_free_sm_and_add_program_for_gpio_range (
    const pio_program_t *program, PIO *pio, uint *sm, uint *offset,
    uint gpio_base, uint gpio_count, bool set_gpio_base)
{
    *pio = pio0;

    int ret = pio_claim_unused_sm (*pio, false);
    if (ret < 0)
    {
        return false;
    }

    *sm = ret;

    ret = pio_add_program(*pio, program);
    if (ret < 0)
    {
        return false;
    }

    *offset = ret;

    return true;
}

static inline void pio_remove_program_and_unclaim_sm (const pio_program_t *program,
                                                      PIO pio, uint sm, uint offset)
{
    pio_remove_program (pio, program, offset);

    pio_sm_unclaim (pio, sm);
}

#endif
