/**
 * @file UEContext.hpp
 * @brief MAC-layer state for a single connected UE.
 */
#pragma once

#include "MacTypes.hpp"

#include <cstdint>
#include <optional>

/**
 * @class UEContext
 * @brief Holds all MAC-layer state needed to schedule a single UE.
 *
 * The scheduler reads CQI and BSR from this object to make resource
 * decisions, and writes back the allocated bits so that the exponential
 * moving average (EMA) throughput stays up-to-date.
 *
 * Usage:
 * @code
 * UEContext ue{0x0001};
 * ue.setCqi(12);
 * ue.setBsr(4096);
 * ue.updateAvgThroughput(0.0);   // not scheduled this TTI
 * ue.updateAvgThroughput(800.0); // 800 bits allocated
 * ue.recordScheduled(42);        // mark TTI 42 as a scheduling event
 * @endcode
 */
class UEContext {
public:
    // ── Constants ────────────────────────────────────────────────
    static constexpr uint8_t kMinCqi   = 1;    ///< Minimum valid CQI
    static constexpr uint8_t kMaxCqi   = 15;   ///< Maximum valid CQI
    static constexpr uint8_t kDefaultCqi = 7;  ///< Mid-range starting CQI
    static constexpr double  kEmaAlpha = 0.1;  ///< EMA smoothing factor

    // ── Constructor ──────────────────────────────────────────────
    /**
     * @brief Construct a UE context with the given RNTI.
     * @param rnti Unique cell radio network temporary identifier.
     */
    explicit UEContext(Rnti rnti);

    // ── Accessors ────────────────────────────────────────────────
    Rnti     rnti()               const noexcept { return m_rnti; }
    uint8_t  cqi()                const noexcept { return m_cqi; }
    uint32_t bsr()                const noexcept { return m_bsr; }
    double   avgThroughput()      const noexcept { return m_avgThroughput; }
    uint32_t schedulingCount()    const noexcept { return m_schedulingCount; }

    /**
     * @brief Bits allocated to this UE in the most recent TTI.
     *
     * Set by updateAvgThroughput() every TTI (0 when not scheduled).
     * Read by Statistics::recordTti() to collect per-TTI throughput
     * without needing access to the scheduler's internal CQI table.
     */
    double lastTtiThroughput() const noexcept { return m_lastTtiThroughput; }

    /**
     * @brief Returns the last TTI in which this UE was scheduled,
     *        or std::nullopt if it has never been scheduled.
     */
    std::optional<TtiNumber> lastScheduledTti() const noexcept { return m_lastScheduledTti; }

    // ── Mutators ─────────────────────────────────────────────────
    /**
     * @brief Update CQI from a periodic UE report.
     * @param cqi Channel quality indicator in [kMinCqi, kMaxCqi].
     * @throws std::out_of_range if the value is outside the valid range.
     */
    void setCqi(uint8_t cqi);

    /**
     * @brief Update the Buffer Status Report.
     * @param bytes Pending bytes in the UE's MAC/RLC queue.
     */
    void setBsr(uint32_t bytes) noexcept;

    /**
     * @brief Update the EMA average throughput.
     *
     * Call this every TTI: pass the allocated bits when the UE is
     * scheduled, or 0 when it is not. Both cases are needed so that
     * the average decays correctly for idle UEs.
     *
     * Formula: avg = (1 - α) × avg + α × instantBits
     *
     * @param instantBits Bits assigned to this UE in the current TTI (≥ 0).
     */
    void updateAvgThroughput(double instantBits) noexcept;

    /**
     * @brief Record that this UE was scheduled at @p tti.
     * @param tti Current TTI number.
     */
    void recordScheduled(TtiNumber tti) noexcept;

private:
    Rnti     m_rnti;
    uint8_t  m_cqi            { kDefaultCqi };
    uint32_t m_bsr            { 0 };
    double   m_avgThroughput  { 0.0 };
    double   m_lastTtiThroughput{ 0.0 };   ///< Bits assigned in the most recent TTI
    uint32_t m_schedulingCount{ 0 };
    std::optional<TtiNumber> m_lastScheduledTti{ std::nullopt };
};
