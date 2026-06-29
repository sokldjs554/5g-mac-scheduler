#include "scheduler/ProportionalFairScheduler.hpp"

#include <algorithm>
#include <cassert>
#include <vector>

void ProportionalFairScheduler::schedule(std::vector<UEContext*>& ues,
                                         ResourceGrid&             grid,
                                         TtiNumber                 tti)
{
    if (ues.empty()) return;

    const int numUes  = static_cast<int>(ues.size());
    const int numPrbs = static_cast<int>(grid.numPrbs());

    // ── Step 1: Compute PF metric for every UE ───────────────────
    struct UeMetric {
        double     metric;
        UEContext* ue;
    };

    std::vector<UeMetric> sorted;
    sorted.reserve(numUes);

    for (UEContext* ue : ues) {
        // Instantaneous rate per PRB at the UE's reported CQI.
        const double rInstant = bitsForPrbs(1, ue->cqi());
        // Guard against zero average with a small epsilon.
        const double rAvg     = std::max(ue->avgThroughput(), kMinAvgThroughput);
        sorted.push_back({ rInstant / rAvg, ue });
    }

    // ── Step 2: Sort descending — highest metric schedules first ─
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const UeMetric& a, const UeMetric& b) {
                         return a.metric > b.metric;
                     });

    // ── Step 3: Distribute PRBs in metric order ──────────────────
    // Same split as Round Robin (equal shares + remainder), but the UE
    // with the highest PF metric receives the first block of PRBs.
    const int baseShare  = numPrbs / numUes;
    const int remainder  = numPrbs % numUes;
    const int nScheduled = std::min(numUes, numPrbs);

    int prbIdx = 0;
    for (int i = 0; i < nScheduled; ++i) {
        UEContext* ue     = sorted[i].ue;
        const int toAlloc = baseShare + (i < remainder ? 1 : 0);

        // Invariant: prbIdx + toAlloc ≤ numPrbs, and all PRBs are free.
        const bool ok = grid.allocateRange(ue->rnti(),
                                           static_cast<PrbIndex>(prbIdx),
                                           static_cast<uint8_t>(toAlloc));
        assert(ok && "PF: allocateRange failed — check PRB count invariant");

        ue->updateAvgThroughput(bitsForPrbs(static_cast<uint8_t>(toAlloc),
                                            ue->cqi()));
        ue->recordScheduled(tti);
        prbIdx += toAlloc;
    }

    // ── Step 4: Decay EMA for UEs not scheduled this TTI ─────────
    for (int i = nScheduled; i < numUes; ++i) {
        sorted[i].ue->updateAvgThroughput(0.0);
    }
}
