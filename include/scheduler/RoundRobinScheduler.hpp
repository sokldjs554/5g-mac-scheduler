/**
 * @file RoundRobinScheduler.hpp
 * @brief Round Robin MAC scheduler.
 */
#pragma once

#include "scheduler/IScheduler.hpp"

#include <cstdint>

/**
 * @class RoundRobinScheduler
 * @brief Distributes PRBs equally among UEs in a rotating order.
 *
 * Each TTI, PRBs are split as evenly as possible across all active UEs.
 * The starting UE advances by one slot every TTI, so each UE gets the
 * "first" contiguous PRB block in turn — preventing starvation when
 * the remainder PRBs cannot be shared perfectly.
 *
 * ### Algorithm (per TTI)
 * 1. Compute **base** = ⌊numPrbs / numUes⌋ and **remainder** = numPrbs mod numUes.
 * 2. Starting from @c m_nextUeIndex, assign @c base+1 PRBs to the first
 *    @c remainder UEs and @c base PRBs to the rest.
 * 3. Advance @c m_nextUeIndex by 1 (mod numUes) for the next TTI.
 *
 * ### Fairness property
 * Over time each UE receives the same total number of PRBs regardless
 * of its channel quality — pure frequency-domain fairness, no CQI awareness.
 */
class RoundRobinScheduler : public IScheduler {
public:
    RoundRobinScheduler() = default;

    /**
     * @brief Schedule one TTI using Round Robin.
     * @param ues  Active UEs (non-null).
     * @param grid Resource grid for this TTI (already reset).
     * @param tti  Current TTI number.
     */
    void schedule(std::vector<UEContext*>& ues,
                  ResourceGrid&             grid,
                  TtiNumber                 tti) override;

private:
    /// Index of the UE that will receive the first block of PRBs next TTI.
    uint32_t m_nextUeIndex{0};
};
