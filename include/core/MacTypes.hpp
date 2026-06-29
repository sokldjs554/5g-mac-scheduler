/**
 * @file MacTypes.hpp
 * @brief Common type aliases used across the MAC layer.
 */
#pragma once

#include <cstdint>

/// @brief Radio Network Temporary Identifier (C-RNTI).
/// Valid range for C-RNTI: 0x0001 – 0xFFFD (3GPP TS 38.321).
using Rnti = uint16_t;

/// @brief Physical Resource Block index (0-based).
using PrbIndex = uint8_t;

/// @brief TTI (Transmission Time Interval) counter.
/// At numerology 0, one TTI equals 1 ms.
using TtiNumber = uint32_t;
