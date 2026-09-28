/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#ifndef FDSWU_PORT_PORT_H
#define FDSWU_PORT_PORT_H

#include <stdbool.h>
#include <stdint.h>

void fdswu_port_start(uint8_t pullups_b, uint8_t pullups_d);
uint16_t fdswu_port_poll(void);
bool fdswu_port_begin_update(void);
void fdswu_port_commit_heads(uint8_t now, uint8_t next_edge, uint8_t later_edge);
uint8_t fdswu_port_head_direction(void);
uint8_t fdswu_port_pending_heads(void);

#endif
