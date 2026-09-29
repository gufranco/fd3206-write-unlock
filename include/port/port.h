/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#ifndef FDSWU_PORT_PORT_H
#define FDSWU_PORT_PORT_H

#include <stdint.h>

void fdswu_port_start(uint8_t pullups_a, uint8_t pullups_b, uint8_t pullups_d);
uint16_t fdswu_port_poll(void);
void fdswu_port_commit_write_lines(uint8_t now_if_clear, uint8_t next_if_clear, uint8_t later_if_clear,
                                   uint8_t now_if_set, uint8_t next_if_set, uint8_t later_if_set);

#endif
