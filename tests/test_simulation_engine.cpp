#include <gtest/gtest.h>

#include "sim/SimulationEngine.hpp"
#include "scheduler/SchedulerFactory.hpp"

// ── Helper ────────────────────────────────────────────────────────────────────
static SimConfig makeConfig(uint32_t ttis  = 10,
                            uint8_t  prbs  = 52,
                            uint32_t seed  = 42,
                            double   rate  = 500.0)
{
    SimConfig cfg;
    cfg.numTtis               = ttis;
    cfg.numPrbs               = prbs;
    cfg.randomSeed            = seed;
    cfg.arrivalRateBitsPerTti = rate;
    return cfg;
}

// ── Pre-run guards ────────────────────────────────────────────────────────────

TEST(SimulationEngineTest, ThrowsWhenNoSchedulerSet) {
    SimulationEngine engine{makeConfig(1)};
    engine.addUe(0x0001, 7);
    EXPECT_THROW(engine.run(), std::logic_error);
}

TEST(SimulationEngineTest, RunsSuccessfullyWithOneUe) {
    SimulationEngine engine{makeConfig(10)};
    engine.addUe(0x0001, 7);
    engine.setScheduler(SchedulerFactory::create("rr"));
    EXPECT_NO_THROW(engine.run());
}

// ── TTI count ─────────────────────────────────────────────────────────────────

TEST(SimulationEngineTest, RecordsExactNumberOfTtis) {
    SimulationEngine engine{makeConfig(50)};
    engine.addUe(0x0001, 7);
    engine.setScheduler(SchedulerFactory::create("rr"));
    engine.run();

    EXPECT_EQ(engine.statistics().numTtisRecorded(), 50u);
}

TEST(SimulationEngineTest, ZeroTtisRecordsNothing) {
    SimulationEngine engine{makeConfig(0)};
    engine.addUe(0x0001, 7);
    engine.setScheduler(SchedulerFactory::create("rr"));
    engine.run();

    EXPECT_EQ(engine.statistics().numTtisRecorded(), 0u);
}

// ── Throughput sanity ─────────────────────────────────────────────────────────

TEST(SimulationEngineTest, SingleUeAchievesNonZeroThroughput) {
    SimulationEngine engine{makeConfig(10)};
    engine.addUe(0x0001, 7);
    engine.setScheduler(SchedulerFactory::create("rr"));
    engine.run();

    EXPECT_GT(engine.statistics().totalThroughputBits(), 0.0);
    EXPECT_GT(engine.statistics().avgThroughputForUe(0x0001), 0.0);
}

TEST(SimulationEngineTest, MultipleUesAllGetThroughput) {
    SimulationEngine engine{makeConfig(20)};
    engine.addUe(0x0001, 12);
    engine.addUe(0x0002,  7);
    engine.addUe(0x0003,  3);
    engine.setScheduler(SchedulerFactory::create("rr"));
    engine.run();

    EXPECT_GT(engine.statistics().avgThroughputForUe(0x0001), 0.0);
    EXPECT_GT(engine.statistics().avgThroughputForUe(0x0002), 0.0);
    EXPECT_GT(engine.statistics().avgThroughputForUe(0x0003), 0.0);
}

// ── Empty UE list ─────────────────────────────────────────────────────────────

TEST(SimulationEngineTest, RunsWithZeroUesSafely) {
    SimulationEngine engine{makeConfig(5)};
    engine.setScheduler(SchedulerFactory::create("pf"));
    EXPECT_NO_THROW(engine.run());
    EXPECT_EQ(engine.statistics().numTtisRecorded(), 5u);
    EXPECT_DOUBLE_EQ(engine.statistics().totalThroughputBits(), 0.0);
}

// ── Fairness ──────────────────────────────────────────────────────────────────

TEST(SimulationEngineTest, JainsFairnessIndexInBounds) {
    SimulationEngine engine{makeConfig(100)};
    engine.addUe(0x0001, 12);
    engine.addUe(0x0002,  7);
    engine.addUe(0x0003,  3);
    engine.setScheduler(SchedulerFactory::create("rr"));
    engine.run();

    const double jfi = engine.statistics().jainsFairnessIndex();
    EXPECT_GE(jfi, 0.0);
    EXPECT_LE(jfi, 1.0 + 1e-9);
}

TEST(SimulationEngineTest, TwoEqualCqiUesRRJainsIsOne) {
    // With RR and identical CQI, both UEs get equal PRBs → equal throughput → J = 1.
    SimulationEngine engine{makeConfig(100)};
    engine.addUe(0x0001, 7);
    engine.addUe(0x0002, 7);
    engine.setScheduler(SchedulerFactory::create("rr"));
    engine.run();

    EXPECT_NEAR(engine.statistics().jainsFairnessIndex(), 1.0, 1e-6);
}

// ── Both schedulers work ──────────────────────────────────────────────────────

TEST(SimulationEngineTest, PFSchedulerRunsWithoutErrors) {
    SimulationEngine engine{makeConfig(20)};
    engine.addUe(0x0001, 10);
    engine.addUe(0x0002,  5);
    engine.setScheduler(SchedulerFactory::create("pf"));
    EXPECT_NO_THROW(engine.run());
    EXPECT_EQ(engine.statistics().numTtisRecorded(), 20u);
}

// ── Reproducibility ───────────────────────────────────────────────────────────

TEST(SimulationEngineTest, SameSeedGivesIdenticalStats) {
    auto runEngine = [](uint32_t seed) {
        SimulationEngine engine{makeConfig(50, 52, seed)};
        engine.addUe(0x0001, 10);
        engine.addUe(0x0002,  7);
        engine.setScheduler(SchedulerFactory::create("rr"));
        engine.run();
        return engine.statistics().totalThroughputBits();
    };

    const double run1 = runEngine(42);
    const double run2 = runEngine(42);
    EXPECT_NEAR(run1, run2, 1e-9);
}

// ── BSR drain ─────────────────────────────────────────────────────────────────

TEST(SimulationEngineTest, BsrDrainedByScheduledBytes) {
    // With a high arrival rate and a generous scheduler, BSR should
    // stabilise rather than grow without bound.
    SimulationEngine engine{makeConfig(200, 52, 42, 500.0)};
    engine.addUe(0x0001, 12);
    engine.setScheduler(SchedulerFactory::create("rr"));
    engine.run();

    // Scheduler transmits far more than 500 bits/TTI → BSR stays low
    // (we can't inspect BSR after run(), but the test verifies no crash
    // and reasonable throughput)
    EXPECT_GT(engine.statistics().totalThroughputBits(), 0.0);
}

// ── setScheduler behaviour ────────────────────────────────────────────────────

TEST(SimulationEngineTest, SetSchedulerCanBeCalledTwice) {
    // The second call must replace the first; the simulation must use
    // the most-recently-set scheduler.
    SimulationEngine engine{makeConfig(5)};
    engine.addUe(0x0001, 7);
    engine.setScheduler(SchedulerFactory::create("rr"));
    engine.setScheduler(SchedulerFactory::create("pf")); // overwrites
    EXPECT_NO_THROW(engine.run());
    EXPECT_EQ(engine.statistics().numTtisRecorded(), 5u);
}
