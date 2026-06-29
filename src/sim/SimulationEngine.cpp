#include "sim/SimulationEngine.hpp"

#include <algorithm>   // std::min
#include <stdexcept>

SimulationEngine::SimulationEngine(const SimConfig& config)
    : m_config    { config }
    , m_trafficGen{ config.arrivalRateBitsPerTti, config.randomSeed }
    , m_grid      { config.numPrbs }
{}

void SimulationEngine::addUe(Rnti rnti, uint8_t cqi)
{
    m_ues.emplace_back(rnti);
    m_ues.back().setCqi(cqi);
    // m_uePtrs is populated in run(), once m_ues is stable
}

void SimulationEngine::setScheduler(std::unique_ptr<IScheduler> scheduler)
{
    m_scheduler = std::move(scheduler);
}

void SimulationEngine::run()
{
    if (!m_scheduler) {
        throw std::logic_error(
            "SimulationEngine::run(): no scheduler has been set. "
            "Call setScheduler() before run().");
    }

    // Build stable raw-pointer list now that m_ues will not grow during the loop.
    m_uePtrs.clear();
    for (auto& ue : m_ues) {
        m_uePtrs.push_back(&ue);
    }

    for (TtiNumber t = 1; t <= m_config.numTtis; ++t) {
        runOneTti(t);
    }
}

void SimulationEngine::runOneTti(TtiNumber tti)
{
    // ── 1. Traffic arrives ────────────────────────────────────────
    // Each UE's BSR grows by a Poisson-distributed number of bytes.
    m_trafficGen.generate(m_uePtrs);

    // ── 2. Fresh resource grid ────────────────────────────────────
    m_grid.reset();

    // ── 3. Scheduler assigns PRBs ─────────────────────────────────
    // Also calls UEContext::updateAvgThroughput(), which sets
    // lastTtiThroughput() — used by the BSR drain and Statistics below.
    m_scheduler->schedule(m_uePtrs, m_grid, tti);

    // ── 4. Drain BSR by bytes transmitted this TTI ────────────────
    for (UEContext* ue : m_uePtrs) {
        const double   bitsTx  = ue->lastTtiThroughput();
        const uint32_t bytesTx = static_cast<uint32_t>(bitsTx / 8.0);
        const uint32_t drained = std::min(ue->bsr(), bytesTx);
        ue->setBsr(ue->bsr() - drained);
    }

    // ── 5. Record per-TTI statistics ─────────────────────────────
    m_stats.recordTti(m_uePtrs, m_grid, tti);
}
