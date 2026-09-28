#include <stdio.h>
#include <stdlib.h>

#include "fdswu/assert.h"

void fdswu_assert_fail(void) {
    fputs("fdswu assertion failed\n", stderr);
    abort();
}
