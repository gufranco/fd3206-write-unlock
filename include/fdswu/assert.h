/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#ifndef FDSWU_ASSERT_H
#define FDSWU_ASSERT_H

void fdswu_assert_fail(void);

#if defined(FDSWU_DEBUG)
#define FDSWU_ASSERT(condition) ((condition) ? (void)0 : fdswu_assert_fail())
#else
#define FDSWU_ASSERT(condition) ((void)sizeof(condition))
#endif

#endif
