/**
 * @file SimulationEngine.hpp
 * @brief Top-level simulation controller.
 */
#pragma once

#include "core/MacTypes.hpp"
#include "core/ResourceGrid.hpp"
#include "core/UEContext.hpp"
#include "scheduler/IScheduler.hpp"
#include "sim/Statistics.hpp"
#include "sim/TrafficGenerator.hpp"

#include <cstdint>
#include <memory>
#include <vector>

/**
 * @struct SimConfig
 * @brief Configuration parameters for one simulation run.
 */
struct SimConfig {
    uint8_t  numPrbs               = 52;      ///< PRBs per TTI (20 MHz NR)
    uint32_t numTtis               = 100;     ///< Number of TTIs to simulate
    double   arrivalRateBitsPerTti = 500.0;   ///< Poisson λ per UE per TTI
    uint32_t randomSeed            = 42;      ///< RNG seed for reproducibility
};

/**
 * @class SimulationEngine
 * @brief Orchestrates the full simulation: traffic, scheduling, and statistics.
 *
 * ### Typical usage
 * @code
 * SimConfig cfg;
 * cfg.numTtis = 200;
 *
 * SimulationEngine engine{cfg};
 * engine.addUe(0x0001, 12);   // RNTI, CQI
 * engine.addUe(0x0002,  5);
 * engine.setScheduler(SchedulerFactory::create("pf"));
 * engine.run();
 * engine.statistics().printReport();
 * @endcode
 *
 * ### TTI loop (per call to run())
 * For each TTI t = 1 … numTtis:
 *  1. **Traffic**: TrafficGenerator::generate() → increments UE BSR.
 *  2. **Reset**: ResourceGrid::reset() → fresh PRB map for this TTI.
 *  3. **Schedule**: IScheduler::schedule() → assigns PRBs, updates UE EMA.
 *  4. **Drain BSR**: subtract transmitted bytes from each UE's BSR.
 *  5. **Record**: Statistics::recordTti() → accumulate metrics.
 *
 * @note run() may only be called once per engine instance.
 *       addUe() must be called before run().
 */
class SimulationEngine {
public:
    /**
     * @brief Construct the engine with the given configuration.
     * @param config Simulation parameters (uses defaults if omitted).
     */
    explicit SimulationEngine(const SimConfig& config = SimConfig{});

    // ── Setup (call before run()) ─────────────────────────────────
    /**
     * @brief Register a UE that will participate in the simulation.
     * @param rnti  Unique identifier for the UE.
     * @param cqi   Initial Channel Quality Indicator [1, 15].
     */
    void addUe(Rnti rnti, uint8_t cqi);

    /**
     * @brief Set the scheduling algorithm.
     * @param scheduler  Owning pointer to a concrete IScheduler.
     */
    void setScheduler(std::unique_ptr<IScheduler> scheduler);

    // ── Run ───────────────────────────────────────────────────────
    /**
     * @brief Execute the full simulation (numTtis TTIs).
     * @throws std::logic_error if no scheduler has been set.
     */
    void run();

    // ── Results ───────────────────────────────────────────────────
    /** @brief Access the collected statistics after run() completes. */
    const Statistics& statistics() const noexcept { return m_stats; }

private:
    void runOneTti(TtiNumber tti);

    SimConfig m_config;

    // UEs are owned here; raw pointers for the scheduler are built in run().
    std::vector<UEContext>   m_ues;
    std::vector<UEContext*>  m_uePtrs;

    std::unique_ptr<IScheduler> m_scheduler;
    TrafficGenerator             m_trafficGen;
    Statistics                   m_stats;
    ResourceGrid                 m_grid;
};
