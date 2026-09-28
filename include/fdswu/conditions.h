/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#ifndef FDSWU_CONDITIONS_H
#define FDSWU_CONDITIONS_H

#include <stdbool.h>
#include <stdint.h>

bool fdswu_conditions_allow(uint8_t port_a_pins, uint8_t port_b_pins);

#endif
