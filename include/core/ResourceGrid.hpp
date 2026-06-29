/**
 * @file ResourceGrid.hpp
 * @brief PRB allocation grid for a single TTI.
 */
#pragma once

#include "MacTypes.hpp"

#include <cstdint>
#include <optional>
#include <vector>

/**
 * @class ResourceGrid
 * @brief Tracks Physical Resource Block (PRB) allocation within one TTI.
 *
 * There is one ResourceGrid per TTI.  At the start of every TTI the
 * scheduler calls reset(), then allocates PRBs to UEs one by one.
 * The grid enforces that each PRB is assigned to at most one UE.
 *
 * Default configuration: 52 PRBs (20 MHz, NR numerology 0).
 *
 * Usage:
 * @code
 * ResourceGrid grid{52};          // one-time construction
 *
 * // --- each TTI ---
 * grid.reset();
 * grid.allocate(0x0001, 0);       // UE 1 gets PRB 0
 * grid.allocateRange(0x0002, 1, 4); // UE 2 gets PRBs 1-4
 * uint8_t remaining = grid.freePrbCount();
 * @endcode
 */
class ResourceGrid {
public:
    static constexpr uint8_t kDefaultNumPrbs = 52; ///< 20 MHz NR band (numerology 0)

    /**
     * @brief Construct a resource grid.
     * @param numPrbs Total PRBs available per TTI (must be > 0).
     * @throws std::invalid_argument if numPrbs is 0.
     */
    explicit ResourceGrid(uint8_t numPrbs = kDefaultNumPrbs);

    // ── TTI lifecycle ────────────────────────────────────────────
    /**
     * @brief Clear all allocations.  Call at the start of every new TTI.
     */
    void reset() noexcept;

    // ── Allocation ───────────────────────────────────────────────
    /**
     * @brief Allocate a single PRB to a UE.
     * @param rnti  UE identifier.
     * @param index PRB index in [0, numPrbs()).
     * @return true on success; false if the PRB is already allocated or out of range.
     */
    [[nodiscard]] bool allocate(Rnti rnti, PrbIndex index) noexcept;

    /**
     * @brief Allocate a contiguous range of PRBs to a UE (atomic).
     *
     * Either all PRBs in the range are allocated, or none are.
     * The operation fails if any PRB in the range is already taken
     * or if the range exceeds the grid boundaries.
     *
     * @param rnti  UE identifier.
     * @param start Index of the first PRB.
     * @param count Number of consecutive PRBs.
     * @return true if all PRBs were free and are now allocated; false otherwise.
     */
    [[nodiscard]] bool allocateRange(Rnti rnti, PrbIndex start, uint8_t count) noexcept;

    // ── Query ────────────────────────────────────────────────────
    /**
     * @brief Returns true if the PRB at @p index is unallocated.
     *        Returns false for out-of-range indices.
     */
    bool isFree(PrbIndex index) const noexcept;

    /**
     * @brief Returns the RNTI of the UE assigned to @p index,
     *        or std::nullopt if the PRB is free or index is out of range.
     */
    std::optional<Rnti> allocatedUe(PrbIndex index) const noexcept;

    /** @brief Number of PRBs not yet assigned in this TTI. */
    uint8_t freePrbCount() const noexcept;

    /** @brief Total PRBs configured for this grid (constant after construction). */
    uint8_t numPrbs() const noexcept { return m_numPrbs; }

private:
    uint8_t m_numPrbs;
    std::vector<std::optional<Rnti>> m_grid; ///< index → RNTI (nullopt = free)
};
