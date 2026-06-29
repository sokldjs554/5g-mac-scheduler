/**
 * @file ProportionalFairScheduler.hpp
 * @brief Proportional Fair MAC scheduler.
 */
#pragma once

#include "scheduler/IScheduler.hpp"

/**
 * @class ProportionalFairScheduler
 * @brief Schedules UEs by the Proportional Fair (PF) metric.
 *
 * The PF metric for UE k at TTI t is:
 * @verbatim
 *              R_instant(k)
 *   M(k) = ─────────────────
 *           max(R_avg(k), ε)
 * @endverbatim
 *
 * where:
 * - @c R_instant(k) is the single-PRB throughput estimated from UE k's CQI
 *   (bits/PRB, from the inherited lookup table).
 * - @c R_avg(k) is UE k's exponential moving average throughput
 *   stored in UEContext (updated every TTI).
 * - @c ε = @c kMinAvgThroughput prevents division by zero in the first TTI.
 *
 * UEs are sorted by M(k) descending, then PRBs are distributed among
 * them in equal shares (same split as Round Robin, but in metric order).
 *
 * ### Fairness property
 * PF tends to equalise @c M(k) across UEs over time: a UE that receives
 * many PRBs sees its @c R_avg grow, reducing its future metric, while a
 * UE starved of PRBs retains a high metric and wins the next contest.
 */
class ProportionalFairScheduler : public IScheduler {
public:
    ProportionalFairScheduler() = default;

    /**
     * @brief Schedule one TTI using the Proportional Fair algorithm.
     * @param ues  Active UEs (non-null).
     * @param grid Resource grid for this TTI (already reset).
     * @param tti  Current TTI number.
     */
    void schedule(std::vector<UEContext*>& ues,
                  ResourceGrid&             grid,
                  TtiNumber                 tti) override;

private:
    /// Minimum value used in the denominator to avoid division by zero.
    static constexpr double kMinAvgThroughput = 1.0;
};
