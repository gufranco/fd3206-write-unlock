#include <limits.h>
#include <stdint.h>

#if defined(__AVR__)
static_assert(CHAR_MIN < 0);
static_assert(sizeof(short) == 2U);
static_assert(sizeof(int) == 2U);
static_assert(sizeof(long) == 4U);
static_assert(sizeof(long long) == 8U);
static_assert(sizeof(void *) == 2U);
static_assert(sizeof(float) == 4U);
static_assert(sizeof(double) == 4U);
static_assert(sizeof(bool) == 1U);
#elif defined(__aarch64__)
static_assert(CHAR_MIN == 0);
static_assert(sizeof(short) == 2U);
static_assert(sizeof(int) == 4U);
static_assert(sizeof(long) == 8U);
static_assert(sizeof(long long) == 8U);
static_assert(sizeof(void *) == 8U);
static_assert(sizeof(float) == 4U);
static_assert(sizeof(double) == 8U);
static_assert(sizeof(bool) == 1U);
#elif defined(__x86_64__)
static_assert(CHAR_MIN < 0);
static_assert(sizeof(short) == 2U);
static_assert(sizeof(int) == 4U);
static_assert(sizeof(long) == 8U);
static_assert(sizeof(long long) == 8U);
static_assert(sizeof(void *) == 8U);
static_assert(sizeof(float) == 4U);
static_assert(sizeof(double) == 8U);
static_assert(sizeof(bool) == 1U);
#else
#error "type widths are recorded for avr, aarch64 and x86_64 only"
#endif

static_assert(sizeof(uint8_t) == 1U);
static_assert(sizeof(uint16_t) == 2U);
static_assert(sizeof(uint32_t) == 4U);
static_assert(sizeof(uint64_t) == 8U);
