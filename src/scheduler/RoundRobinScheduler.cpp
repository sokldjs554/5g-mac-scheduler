#include "scheduler/RoundRobinScheduler.hpp"

#include <algorithm>
#include <cassert>

void RoundRobinScheduler::schedule(std::vector<UEContext*>& ues,
                                   ResourceGrid&             grid,
                                   TtiNumber                 tti)
{
    if (ues.empty()) return;

    const int numUes  = static_cast<int>(ues.size());
    const int numPrbs = static_cast<int>(grid.numPrbs());

    // Keep m_nextUeIndex valid if the UE count changes between TTIs.
    m_nextUeIndex %= static_cast<uint32_t>(numUes);

    // ── PRB distribution ─────────────────────────────────────────
    // baseShare PRBs per UE, with the first `remainder` UEs getting +1.
    const int baseShare  = numPrbs / numUes;
    const int remainder  = numPrbs % numUes;
    // When numPrbs < numUes, only numPrbs UEs are scheduled (1 PRB each).
    const int nScheduled = std::min(numUes, numPrbs);

    int prbIdx = 0;
    for (int i = 0; i < nScheduled; ++i) {
        const int ueIdx   = (static_cast<int>(m_nextUeIndex) + i) % numUes;
        UEContext* ue     = ues[ueIdx];
        const int toAlloc = baseShare + (i < remainder ? 1 : 0);

        // Invariant: prbIdx + toAlloc ≤ numPrbs, and all PRBs are free
        // (grid was reset by SimulationEngine before this call).
        const bool ok = grid.allocateRange(ue->rnti(),
                                           static_cast<PrbIndex>(prbIdx),
                                           static_cast<uint8_t>(toAlloc));
        assert(ok && "RR: allocateRange failed — check PRB count invariant");

        ue->updateAvgThroughput(bitsForPrbs(static_cast<uint8_t>(toAlloc),
                                            ue->cqi()));
        ue->recordScheduled(tti);
        prbIdx += toAlloc;
    }

    // ── Decay EMA for UEs that could not be scheduled this TTI ───
    for (int i = nScheduled; i < numUes; ++i) {
        const int ueIdx = (static_cast<int>(m_nextUeIndex) + i) % numUes;
        ues[ueIdx]->updateAvgThroughput(0.0);
    }

    // ── Advance the rotation pointer ─────────────────────────────
    m_nextUeIndex = (m_nextUeIndex + 1) % static_cast<uint32_t>(numUes);
}
