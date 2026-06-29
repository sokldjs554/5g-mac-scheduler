#include <iostream>

#include "sim/SimulationEngine.hpp"
#include "scheduler/SchedulerFactory.hpp"

int main()
{
    std::cout << "5G gNodeB MAC Scheduler Simulator  v0.4.0\n"
              << "Milestone 4: Simulation Layer\n\n";

    // ── Shared configuration ──────────────────────────────────────
    SimConfig cfg;
    cfg.numPrbs               = 52;      // 20 MHz NR band
    cfg.numTtis               = 200;
    cfg.arrivalRateBitsPerTti = 1000.0;  // 1 Mbps per UE (at 1ms TTI)
    cfg.randomSeed            = 42;

    // Add three UEs with different channel quality (CQI 12, 7, 3)
    auto addUes = [](SimulationEngine& eng) {
        eng.addUe(0x0001, 12);   // good channel
        eng.addUe(0x0002,  7);   // average
        eng.addUe(0x0003,  3);   // poor channel
    };

    // ── Round Robin ───────────────────────────────────────────────
    std::cout << "Running Round Robin  (200 TTIs, 3 UEs, 52 PRBs) ...\n";
    SimulationEngine rrEngine{cfg};
    addUes(rrEngine);
    rrEngine.setScheduler(SchedulerFactory::create("rr"));
    rrEngine.run();
    rrEngine.statistics().printReport();

    // ── Proportional Fair ─────────────────────────────────────────
    std::cout << "Running Proportional Fair (200 TTIs, 3 UEs, 52 PRBs) ...\n";
    SimulationEngine pfEngine{cfg};
    addUes(pfEngine);
    pfEngine.setScheduler(SchedulerFactory::create("pf"));
    pfEngine.run();
    pfEngine.statistics().printReport();

    return 0;
}
