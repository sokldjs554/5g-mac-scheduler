/**
 * @file IScheduler.hpp
 * @brief Abstract interface for MAC downlink schedulers.
 *
 * Implements the **Strategy** pattern: concrete schedulers (RR, PF, …)
 * all conform to this interface, letting the caller swap algorithms at
 * runtime without changing the surrounding code.
 */
#pragma once

#include "core/MacTypes.hpp"
#include "core/ResourceGrid.hpp"
#include "core/UEContext.hpp"

#include <array>
#include <vector>

/**
 * @class IScheduler
 * @brief Common interface that every downlink MAC scheduler must implement.
 *
 * A scheduler's sole responsibility is to decide, for a single TTI,
 * which UEs receive which PRBs.  It must also update each UE's EMA
 * average throughput so that higher-layer algorithms (e.g. PF metric)
 * remain accurate.
 *
 * Concrete schedulers inherit the CQI → bits-per-PRB lookup table and
 * the helper function @c bitsForPrbs() from this class.
 */
class IScheduler {
public:
    virtual ~IScheduler() = default;

    /**
     * @brief Run the scheduling algorithm for one TTI.
     *
     * Implementations must:
     *  - Call ResourceGrid::allocateRange() to assign PRBs.
     *  - Call UEContext::updateAvgThroughput() for **every** UE
     *    (pass the allocated bits for scheduled UEs, 0 for skipped UEs).
     *  - Call UEContext::recordScheduled() for each UE that receives PRBs.
     *
     * The caller is responsible for calling ResourceGrid::reset() before
     * invoking schedule() each TTI.
     *
     * @param ues  Active UEs available for scheduling (non-null pointers).
     * @param grid TTI resource grid (already reset for this TTI).
     * @param tti  Current TTI number, forwarded to UEContext::recordScheduled().
     */
    virtual void schedule(std::vector<UEContext*>& ues,
                          ResourceGrid&             grid,
                          TtiNumber                 tti) = 0;

protected:
    // ── CQI → throughput mapping ────────────────────────────────
    /**
     * @brief Compute approximate bits delivered by @p numPrbs PRBs at @p cqi.
     *
     * Uses the lookup table below (3GPP TS 38.214 Table 5.2.2.1-3, simplified).
     *
     * @param numPrbs Number of allocated Physical Resource Blocks.
     * @param cqi     Channel Quality Indicator in [1, 15]; returns 0 for CQI 0.
     * @return        Total bits for this allocation.
     */
    static double bitsForPrbs(uint8_t numPrbs, uint8_t cqi) noexcept {
        if (cqi == 0 || cqi > 15) return 0.0;
        return kBitsPerPrb[cqi] * static_cast<double>(numPrbs);
    }

    /**
     * @brief Approximate bits-per-PRB for each CQI level (0–15).
     *
     * Derived from 3GPP TS 38.214 Table 5.2.2.1-3 assuming 12 subcarriers,
     * 14 OFDM symbols, and ~156 data resource elements per PRB (simplified).
     *
     * CQI  0 is undefined (out-of-range).
     * CQI  1– 6: QPSK.
     * CQI  7– 9: 16-QAM.
     * CQI 10–15: 64-QAM.
     */
    static constexpr std::array<double, 16> kBitsPerPrb = {
          0.0,  // CQI  0: undefined
         24.0,  // CQI  1: QPSK  r=0.076
         38.0,  // CQI  2: QPSK  r=0.117
         60.0,  // CQI  3: QPSK  r=0.188
         96.0,  // CQI  4: QPSK  r=0.301
        140.0,  // CQI  5: QPSK  r=0.438
        188.0,  // CQI  6: QPSK  r=0.588
        236.0,  // CQI  7: 16QAM r=0.369
        306.0,  // CQI  8: 16QAM r=0.479
        384.0,  // CQI  9: 16QAM r=0.601
        437.0,  // CQI 10: 64QAM r=0.455
        532.0,  // CQI 11: 64QAM r=0.554
        624.0,  // CQI 12: 64QAM r=0.650
        724.0,  // CQI 13: 64QAM r=0.754
        818.0,  // CQI 14: 64QAM r=0.853
        889.0,  // CQI 15: 64QAM r=0.926
    };
};
