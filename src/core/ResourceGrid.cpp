#include "core/ResourceGrid.hpp"

#include <algorithm>
#include <stdexcept>

ResourceGrid::ResourceGrid(uint8_t numPrbs)
    : m_numPrbs{numPrbs}
    , m_grid(numPrbs, std::nullopt)
{
    if (numPrbs == 0) {
        throw std::invalid_argument("ResourceGrid: numPrbs must be > 0");
    }
}

void ResourceGrid::reset() noexcept {
    std::fill(m_grid.begin(), m_grid.end(), std::nullopt);
}

bool ResourceGrid::allocate(Rnti rnti, PrbIndex index) noexcept {
    if (index >= m_numPrbs) return false;
    if (m_grid[index].has_value()) return false;
    m_grid[index] = rnti;
    return true;
}

bool ResourceGrid::allocateRange(Rnti rnti, PrbIndex start, uint8_t count) noexcept {
    if (count == 0) return false;

    // Use int arithmetic to avoid uint8_t overflow in range check.
    if (static_cast<int>(start) + count > m_numPrbs) return false;

    // Check atomically: all PRBs in range must be free.
    for (int i = start; i < start + count; ++i) {
        if (m_grid[i].has_value()) return false;
    }

    // All clear — commit the allocation.
    for (int i = start; i < start + count; ++i) {
        m_grid[i] = rnti;
    }
    return true;
}

bool ResourceGrid::isFree(PrbIndex index) const noexcept {
    if (index >= m_numPrbs) return false;
    return !m_grid[index].has_value();
}

std::optional<Rnti> ResourceGrid::allocatedUe(PrbIndex index) const noexcept {
    if (index >= m_numPrbs) return std::nullopt;
    return m_grid[index];
}

uint8_t ResourceGrid::freePrbCount() const noexcept {
    uint8_t count = 0;
    for (const auto& slot : m_grid) {
        if (!slot.has_value()) ++count;
    }
    return count;
}
