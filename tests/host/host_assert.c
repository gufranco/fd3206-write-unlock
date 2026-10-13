/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "fdswu/assert.h"

void fdswu_assert_fail(uint16_t line) {
    fprintf(stderr, "fdswu assertion failed at line %u\n", (unsigned)line);
    abort();
}
