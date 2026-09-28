/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#if defined(__AVR__)
_Static_assert(CHAR_MIN < 0, "CHAR_MIN < 0");
_Static_assert(sizeof(short) == 2U, "sizeof(short) == 2U");
_Static_assert(sizeof(int) == 2U, "sizeof(int) == 2U");
_Static_assert(sizeof(long) == 4U, "sizeof(long) == 4U");
_Static_assert(sizeof(long long) == 8U, "sizeof(long long) == 8U");
_Static_assert(sizeof(void *) == 2U, "sizeof(void *) == 2U");
_Static_assert(sizeof(float) == 4U, "sizeof(float) == 4U");
_Static_assert(sizeof(double) == 4U, "sizeof(double) == 4U");
_Static_assert(sizeof(bool) == 1U, "sizeof(bool) == 1U");
#elif defined(__aarch64__)
_Static_assert(CHAR_MIN == 0, "CHAR_MIN == 0");
_Static_assert(sizeof(short) == 2U, "sizeof(short) == 2U");
_Static_assert(sizeof(int) == 4U, "sizeof(int) == 4U");
_Static_assert(sizeof(long) == 8U, "sizeof(long) == 8U");
_Static_assert(sizeof(long long) == 8U, "sizeof(long long) == 8U");
_Static_assert(sizeof(void *) == 8U, "sizeof(void *) == 8U");
_Static_assert(sizeof(float) == 4U, "sizeof(float) == 4U");
_Static_assert(sizeof(double) == 8U, "sizeof(double) == 8U");
_Static_assert(sizeof(bool) == 1U, "sizeof(bool) == 1U");
#elif defined(__x86_64__)
_Static_assert(CHAR_MIN < 0, "CHAR_MIN < 0");
_Static_assert(sizeof(short) == 2U, "sizeof(short) == 2U");
_Static_assert(sizeof(int) == 4U, "sizeof(int) == 4U");
_Static_assert(sizeof(long) == 8U, "sizeof(long) == 8U");
_Static_assert(sizeof(long long) == 8U, "sizeof(long long) == 8U");
_Static_assert(sizeof(void *) == 8U, "sizeof(void *) == 8U");
_Static_assert(sizeof(float) == 4U, "sizeof(float) == 4U");
_Static_assert(sizeof(double) == 8U, "sizeof(double) == 8U");
_Static_assert(sizeof(bool) == 1U, "sizeof(bool) == 1U");
#else
#error "type widths are recorded for avr, aarch64 and x86_64 only"
#endif

_Static_assert(sizeof(uint8_t) == 1U, "sizeof(uint8_t) == 1U");
_Static_assert(sizeof(uint16_t) == 2U, "sizeof(uint16_t) == 2U");
_Static_assert(sizeof(uint32_t) == 4U, "sizeof(uint32_t) == 4U");
_Static_assert(sizeof(uint64_t) == 8U, "sizeof(uint64_t) == 8U");
