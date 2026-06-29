/**
 * @file HARQManager.hpp
 * @brief Downlink HARQ process manager for a single UE.
 */
#pragma once

#include <array>
#include <cstdint>
#include <optional>

/**
 * @enum HarqProcessState
 * @brief Life-cycle states of a single HARQ process.
 */
enum class HarqProcessState : uint8_t {
    Idle,       ///< No pending transmission; available for new data.
    WaitingAck, ///< Data sent; awaiting ACK or NACK feedback from the UE.
    Nacked,     ///< NACK received; retransmission is required.
};

/**
 * @struct HarqProcess
 * @brief Per-process state record.
 */
struct HarqProcess {
    HarqProcessState state    { HarqProcessState::Idle };
    uint8_t          retxCount{ 0 }; ///< Number of retransmissions so far.
};

/**
 * @class HARQManager
 * @brief Manages the 8 downlink HARQ processes for a single UE.
 *
 * Each UE has up to 8 independent HARQ processes running in parallel
 * (3GPP TS 38.321, Section 5.4).  The scheduler picks an idle process
 * for each new transport block and tracks its state through the
 * ACK/NACK feedback loop.
 *
 * State machine per process:
 * @verbatim
 *   Idle ──startTransmission()──► WaitingAck
 *                                    │
 *                         ┌──────────┴──────────┐
 *                  receiveAck()          receiveNack()
 *                         │                     │
 *                         ▼                     ▼
 *                        Idle               Nacked
 *                                              │
 *                                        retransmit()
 *                                              │
 *                                   ┌──────────┴───────────────┐
 *                              count ≤ max               count > max
 *                                   │                          │
 *                                   ▼                          ▼
 *                               WaitingAck                   Idle (flush)
 * @endverbatim
 *
 * After kMaxRetransmissions NACKs the packet is flushed and the process
 * returns to Idle (upper-layer ARQ handles recovery).
 */
class HARQManager {
public:
    static constexpr uint8_t kNumProcesses       = 8; ///< DL HARQ processes per UE (3GPP)
    static constexpr uint8_t kMaxRetransmissions = 4; ///< Max retransmissions before flush

    HARQManager() = default;

    // ── New transmission ─────────────────────────────────────────
    /**
     * @brief Find the lowest-indexed process that is currently Idle.
     * @return Process ID in [0, kNumProcesses), or std::nullopt if all busy.
     */
    [[nodiscard]] std::optional<uint8_t> findIdleProcess() const noexcept;

    /**
     * @brief Start a new transmission on an Idle process (Idle → WaitingAck).
     * @param pid Process ID in [0, kNumProcesses).
     * @return true on success; false if pid is invalid or the process is not Idle.
     */
    [[nodiscard]] bool startTransmission(uint8_t pid) noexcept;

    // ── ACK / NACK handling ──────────────────────────────────────
    /**
     * @brief Handle an ACK: reset the process to Idle.
     * @param pid Process ID.
     */
    void receiveAck(uint8_t pid) noexcept;

    /**
     * @brief Handle a NACK: move a WaitingAck process to Nacked.
     * @param pid Process ID.
     */
    void receiveNack(uint8_t pid) noexcept;

    // ── Retransmission ───────────────────────────────────────────
    /**
     * @brief Schedule a retransmission for a Nacked process (Nacked → WaitingAck).
     * @param pid Process ID.
     * @return true  if the retransmission was scheduled (retxCount ≤ kMaxRetransmissions).
     * @return false if the maximum retransmission count has been exceeded;
     *               in this case the process is reset to Idle (packet flushed).
     */
    [[nodiscard]] bool retransmit(uint8_t pid) noexcept;

    // ── Query ────────────────────────────────────────────────────
    /** @brief Current state of the process at @p pid. Returns Idle for invalid pid. */
    HarqProcessState state(uint8_t pid) const noexcept;

    /** @brief Number of retransmissions performed for @p pid. Returns 0 for invalid pid. */
    uint8_t retxCount(uint8_t pid) const noexcept;

    /** @brief Returns true if at least one process is in the Nacked state. */
    bool hasNackedProcess() const noexcept;

private:
    std::array<HarqProcess, kNumProcesses> m_processes{};
};
