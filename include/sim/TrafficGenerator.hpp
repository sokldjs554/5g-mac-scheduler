/**
 * @file TrafficGenerator.hpp
 * @brief Poisson-process downlink traffic generator.
 */
#pragma once

#include "core/UEContext.hpp"

#include <cstdint>
#include <random>
#include <vector>

/**
 * @class TrafficGenerator
 * @brief Models bursty downlink traffic using a Poisson arrival process.
 *
 * Each call to generate() draws an independent Poisson sample for every UE
 * and adds the result (converted to bytes) to the UE's Buffer Status Report.
 * The generator is seeded for fully deterministic, reproducible simulations.
 *
 * ### Arrival model
 * For each UE in each TTI:
 * @verbatim
 *   arrivedBits ~ Poisson(λ)   where  λ = arrivalRateBitsPerTti
 *   BSR += ceil(arrivedBits / 8)   [bits → bytes]
 * @endverbatim
 *
 * ### Typical parameters
 * | Target rate | arrivalRateBitsPerTti |
 * |-------------|----------------------|
 * |   1 Mbps    |        1000          |
 * |   5 Mbps    |        5000          |
 * |  10 Mbps    |       10000          |
 */
class TrafficGenerator {
public:
    /**
     * @brief Construct the generator.
     * @param arrivalRateBitsPerTti  Poisson mean (λ) in bits per TTI per UE.
     *                               Must be > 0; values < 1 are clamped to 1.
     * @param seed                   RNG seed for reproducibility.
     */
    explicit TrafficGenerator(double   arrivalRateBitsPerTti = 1000.0,
                              uint32_t seed                  = 42);

    /**
     * @brief Generate one TTI of traffic for every UE and update their BSR.
     * @param ues  Active UEs whose BSR will be increased.
     */
    void generate(std::vector<UEContext*>& ues);

    /** @brief Returns the configured arrival rate (λ). */
    double arrivalRateBitsPerTti() const noexcept { return m_arrivalRate; }

private:
    double m_arrivalRate;
    std::mt19937                         m_rng;
    std::poisson_distribution<uint32_t>  m_dist;
};
