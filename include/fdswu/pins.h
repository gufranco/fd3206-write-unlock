#ifndef FDSWU_PINS_H
#define FDSWU_PINS_H

#include <stdint.h>

static constexpr uint8_t FDSWU_HEAD1_MASK = 0b0000'1000U;
static constexpr uint8_t FDSWU_HEAD2_MASK = 0b0000'0100U;
static constexpr uint8_t FDSWU_HEADS_MASK = 0b0000'1100U;
static constexpr uint8_t FDSWU_READY_MASK = 0b0000'0010U;
static constexpr uint8_t FDSWU_GATE_AND_PROTECT_MASK = 0b0000'0011U;
static constexpr uint8_t FDSWU_UNUSED_PULLUPS_B = 0b1111'0001U;
static constexpr uint8_t FDSWU_UNUSED_PULLUPS_D = 0b0111'1011U;

static_assert((FDSWU_HEAD1_MASK | FDSWU_HEAD2_MASK) == FDSWU_HEADS_MASK);
static_assert((FDSWU_HEAD1_MASK & FDSWU_HEAD2_MASK) == 0U);
static_assert((FDSWU_UNUSED_PULLUPS_B & (FDSWU_HEADS_MASK | FDSWU_READY_MASK)) == 0U);

#endif
