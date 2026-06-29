/**
 * @file Statistics.hpp
 * @brief Per-TTI throughput collector and fairness analyser.
 */
#pragma once

#include "core/MacTypes.hpp"
#include "core/ResourceGrid.hpp"
#include "core/UEContext.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

/**
 * @class Statistics
 * @brief Collects scheduling results every TTI and computes summary metrics.
 *
 * Call recordTti() once per TTI (after the scheduler has run).  When the
 * simulation finishes, printReport() prints a formatted summary to stdout.
 *
 * ### Metrics computed
 * - **Per-UE average throughput** (bits/TTI) = cumulative bits / numTtis
 * - **System throughput** (bits/TTI) = Σ per-UE averages
 * - **Jain's Fairness Index**: J = (Σxᵢ)² / (n × Σxᵢ²), where xᵢ is the
 *   cumulative bits for UE i.  Range [1/n, 1]; 1 = perfectly fair.
 */
class Statistics {
public:
    Statistics() = default;

    // ── Data collection ──────────────────────────────────────────
    /**
     * @brief Record scheduling results for one TTI.
     *
     * Reads UEContext::lastTtiThroughput() for per-UE bits and counts
     * PRBs from the allocated resource grid.
     *
     * @param ues  Active UEs (already scheduled for this TTI).
     * @param grid Resource grid after allocation for this TTI.
     * @param tti  Current TTI number (for ordering; stored but not used in metrics).
     */
    void recordTti(const std::vector<UEContext*>& ues,
                   const ResourceGrid&             grid,
                   TtiNumber                       tti);

    // ── Output ───────────────────────────────────────────────────
    /** @brief Print a formatted report to stdout. */
    void printReport() const;

    // ── Accessors ────────────────────────────────────────────────
    /** @brief Total bits transmitted across all UEs and all TTIs. */
    double totalThroughputBits() const noexcept;

    /** @brief Average system bits per TTI (sum over all UEs). */
    double avgSystemThroughputPerTti() const noexcept;

    /**
     * @brief Jain's Fairness Index based on per-UE cumulative throughput.
     *
     * Returns 1.0 if no UEs have been recorded (trivially fair).
     */
    double jainsFairnessIndex() const noexcept;

    /** @brief Number of TTIs recorded so far. */
    uint32_t numTtisRecorded() const noexcept { return m_numTtis; }

    /**
     * @brief Average bits per TTI for one UE.
     * @return 0.0 if the RNTI is unknown or no TTIs have been recorded.
     */
    double avgThroughputForUe(Rnti rnti) const noexcept;

private:
    // ── Per-UE accumulated data ───────────────────────────────────
    struct UeRecord {
        Rnti     rnti           { 0 };
        double   cumulativeBits { 0.0 };
        uint32_t prbsAllocated  { 0 };
        uint32_t schedulingCount{ 0 };
    };

    uint32_t m_numTtis{ 0 };
    std::unordered_map<Rnti, UeRecord> m_ueRecords;
    std::vector<double>                m_ttiThroughput;  ///< system bits per TTI
};
