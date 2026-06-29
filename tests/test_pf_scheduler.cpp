#include <gtest/gtest.h>

#include "scheduler/ProportionalFairScheduler.hpp"
#include "scheduler/RoundRobinScheduler.hpp"
#include "scheduler/SchedulerFactory.hpp"
#include "core/HARQManager.hpp"
#include "core/ResourceGrid.hpp"
#include "core/UEContext.hpp"

// ── Helpers ──────────────────────────────────────────────────────
static UEContext makeUe(Rnti rnti, uint8_t cqi) {
    UEContext ue{rnti};
    ue.setCqi(cqi);
    return ue;
}

static int countPrbs(const ResourceGrid& g, Rnti rnti) {
    int n = 0;
    for (uint8_t i = 0; i < g.numPrbs(); ++i) {
        auto r = g.allocatedUe(i);
        if (r.has_value() && r.value() == rnti) ++n;
    }
    return n;
}

// ── Fixture ──────────────────────────────────────────────────────
class PFSchedulerTest : public ::testing::Test {
protected:
    static constexpr uint8_t kNumPrbs = 10;

    ProportionalFairScheduler sched;
    ResourceGrid              grid{kNumPrbs};
};

// ── Edge cases ───────────────────────────────────────────────────
TEST_F(PFSchedulerTest, EmptyUeListIsNoOp) {
    std::vector<UEContext*> ues;
    EXPECT_NO_THROW(sched.schedule(ues, grid, 1));
    EXPECT_EQ(grid.freePrbCount(), kNumPrbs);
}

TEST_F(PFSchedulerTest, SingleUeReceivesAllPrbs) {
    UEContext ue = makeUe(0x0001, 8);
    std::vector<UEContext*> ues{&ue};

    sched.schedule(ues, grid, 1);

    EXPECT_EQ(grid.freePrbCount(), 0u);
    EXPECT_EQ(ue.schedulingCount(), 1u);
}

// ── PF metric ordering ───────────────────────────────────────────
// When all UEs start with avgThroughput = 0, PF behaves like Max-C/I:
// the UE with the highest CQI (→ highest R_instant) wins the metric contest.

TEST_F(PFSchedulerTest, SchedulesHighestCqiFirstWhenHistoriesEqual) {
    // Listed LOW CQI first intentionally — PF should reorder.
    UEContext ueLow  = makeUe(0x0001, 4);   // R_instant(1PRB) = 96 bits
    UEContext ueHigh = makeUe(0x0002, 12);  // R_instant(1PRB) = 624 bits
    std::vector<UEContext*> ues{&ueLow, &ueHigh};

    sched.schedule(ues, grid, 1);

    // PRB 0 must belong to the UE with the higher metric (ueHigh)
    ASSERT_TRUE(grid.allocatedUe(0).has_value());
    EXPECT_EQ(grid.allocatedUe(0).value(), ueHigh.rnti());
}

TEST_F(PFSchedulerTest, MetricOrderingHoldsForThreeUes) {
    UEContext ue1 = makeUe(0x0001, 15); // metric = 889
    UEContext ue2 = makeUe(0x0002,  8); // metric = 306
    UEContext ue3 = makeUe(0x0003,  2); // metric =  38

    // Listed out of metric order
    std::vector<UEContext*> ues{&ue3, &ue2, &ue1};

    sched.schedule(ues, grid, 1);

    // ue1 (highest metric) should own the first PRBs
    ASSERT_TRUE(grid.allocatedUe(0).has_value());
    EXPECT_EQ(grid.allocatedUe(0).value(), ue1.rnti());
}

// ── PF fairness: unscheduled UE rises in priority ────────────────
//
// Setup:  3 UEs, 2 PRBs (fewer PRBs than UEs so one is skipped per TTI).
//   TTI 1:  ue1 (CQI=7, metric=236) and ue2 (CQI=5, metric=140) are scheduled.
//           ue3 (CQI=3, metric=60) is skipped → avgThroughput stays 0.
//   TTI 2:  ue3's metric = 60 / 1 = 60,  ue1's metric ≈ 236/23.6 = 10.0
//           ue3 now has the highest metric → scheduled first.

TEST_F(PFSchedulerTest, PreviouslySkippedUeGetsHigherPriorityNextTti) {
    UEContext ue1 = makeUe(0x0001, 7);  // highest CQI in this group
    UEContext ue2 = makeUe(0x0002, 5);
    UEContext ue3 = makeUe(0x0003, 3);  // lowest CQI

    ResourceGrid tinyGrid{2}; // 2 PRBs, 3 UEs → one always skipped
    std::vector<UEContext*> ues{&ue1, &ue2, &ue3};

    // TTI 1: ue3 has the lowest metric and is skipped
    sched.schedule(ues, tinyGrid, 1);
    EXPECT_EQ(ue3.schedulingCount(), 0u) << "ue3 should be skipped in TTI 1";

    // TTI 2: ue3 has avgThroughput=0, so its metric surges past the others
    tinyGrid.reset();
    sched.schedule(ues, tinyGrid, 2);

    ASSERT_TRUE(tinyGrid.allocatedUe(0).has_value());
    EXPECT_EQ(tinyGrid.allocatedUe(0).value(), ue3.rnti())
        << "ue3 should win the first PRB block in TTI 2 (highest metric)";
    EXPECT_EQ(ue3.schedulingCount(), 1u);
}

TEST_F(PFSchedulerTest, AllUesGetScheduledEventually) {
    UEContext ue1 = makeUe(0x0001, 7);
    UEContext ue2 = makeUe(0x0002, 5);
    UEContext ue3 = makeUe(0x0003, 3);

    ResourceGrid tinyGrid{2};
    std::vector<UEContext*> ues{&ue1, &ue2, &ue3};

    // Run several TTIs
    for (TtiNumber t = 1; t <= 6; ++t) {
        tinyGrid.reset();
        sched.schedule(ues, tinyGrid, t);
    }

    // Every UE must have been scheduled at least once
    EXPECT_GT(ue1.schedulingCount(), 0u);
    EXPECT_GT(ue2.schedulingCount(), 0u);
    EXPECT_GT(ue3.schedulingCount(), 0u);
}

// ── Throughput accounting ─────────────────────────────────────────
TEST_F(PFSchedulerTest, ScheduledUeAvgThroughputGrows) {
    UEContext ue = makeUe(0x0001, 7);
    std::vector<UEContext*> ues{&ue};

    EXPECT_DOUBLE_EQ(ue.avgThroughput(), 0.0);
    sched.schedule(ues, grid, 1);
    EXPECT_GT(ue.avgThroughput(), 0.0);
}

TEST_F(PFSchedulerTest, UnscheduledUeAvgThroughputDecays) {
    UEContext ue1 = makeUe(0x0001, 7);
    UEContext ue2 = makeUe(0x0002, 7);
    UEContext ue3 = makeUe(0x0003, 7);

    // Give ue3 some history so its metric doesn't win
    ue3.updateAvgThroughput(10000.0); // large history → low metric

    ResourceGrid tinyGrid{2};
    std::vector<UEContext*> ues{&ue1, &ue2, &ue3};

    double prevAvg = ue3.avgThroughput();
    sched.schedule(ues, tinyGrid, 1); // ue3 skipped (lowest metric)
    EXPECT_LT(ue3.avgThroughput(), prevAvg); // EMA decays toward 0
}

// ══════════════════════════════════════════════════════════════════
// PF vs RR Comparison Tests
// ══════════════════════════════════════════════════════════════════

// Key difference:
//   RR assigns PRBs in list order (ignores CQI).
//   PF assigns PRBs by metric order (prioritises high CQI when histories equal).

TEST(SchedulerComparison, RRIgnoresCqiInOrdering) {
    // UEs listed in a specific order: LOW CQI first
    UEContext ueLow  = makeUe(0x0001, 4);
    UEContext ueHigh = makeUe(0x0002, 12);
    std::vector<UEContext*> ues{&ueLow, &ueHigh};

    ResourceGrid grid{10};
    RoundRobinScheduler rr;
    rr.schedule(ues, grid, 1);

    // RR: first in list → PRB 0 belongs to ueLow
    ASSERT_TRUE(grid.allocatedUe(0).has_value());
    EXPECT_EQ(grid.allocatedUe(0).value(), ueLow.rnti())
        << "RR should allocate first PRBs to the first UE in the list";
}

TEST(SchedulerComparison, PFPrioritisesHighCqiWhenHistoriesEqual) {
    // Same list order as the RR test above — PF should reorder
    UEContext ueLow  = makeUe(0x0001, 4);
    UEContext ueHigh = makeUe(0x0002, 12);
    std::vector<UEContext*> ues{&ueLow, &ueHigh};

    ResourceGrid grid{10};
    ProportionalFairScheduler pf;
    pf.schedule(ues, grid, 1);

    // PF: highest CQI wins → PRB 0 belongs to ueHigh
    ASSERT_TRUE(grid.allocatedUe(0).has_value());
    EXPECT_EQ(grid.allocatedUe(0).value(), ueHigh.rnti())
        << "PF should allocate first PRBs to the UE with the highest metric";
}

TEST(SchedulerComparison, RRGivesEqualPrbsRegardlessOfCqi) {
    UEContext ueLow  = makeUe(0x0001, 1);
    UEContext ueHigh = makeUe(0x0002, 15);
    std::vector<UEContext*> ues{&ueLow, &ueHigh};

    ResourceGrid grid{10};
    RoundRobinScheduler rr;
    rr.schedule(ues, grid, 1);

    EXPECT_EQ(countPrbs(grid, 0x0001), 5);  // equal regardless of CQI
    EXPECT_EQ(countPrbs(grid, 0x0002), 5);
}

TEST(SchedulerComparison, PFGivesMorePrbsToHighCqiInFirstTti) {
    // 3 UEs, only 2 PRBs: PF skips the lowest-metric UE
    UEContext ueBest   = makeUe(0x0001, 15); // highest metric
    UEContext ueMid    = makeUe(0x0002,  8);
    UEContext ueWorst  = makeUe(0x0003,  1); // lowest metric

    ResourceGrid grid{2};
    std::vector<UEContext*> ues{&ueBest, &ueMid, &ueWorst};

    ProportionalFairScheduler pf;
    pf.schedule(ues, grid, 1);

    EXPECT_EQ(ueBest.schedulingCount(),  1u); // scheduled
    EXPECT_EQ(ueMid.schedulingCount(),   1u); // scheduled
    EXPECT_EQ(ueWorst.schedulingCount(), 0u); // skipped (lowest metric)
}

// ══════════════════════════════════════════════════════════════════
// SchedulerFactory Tests
// ══════════════════════════════════════════════════════════════════

TEST(SchedulerFactory, CreatesRRScheduler) {
    auto s = SchedulerFactory::create("rr");
    ASSERT_NE(s, nullptr);
}

TEST(SchedulerFactory, CreatesPFScheduler) {
    auto s = SchedulerFactory::create("pf");
    ASSERT_NE(s, nullptr);
}

TEST(SchedulerFactory, UnknownTypeThrows) {
    EXPECT_THROW(SchedulerFactory::create("maxci"), std::invalid_argument);
    EXPECT_THROW(SchedulerFactory::create(""),      std::invalid_argument);
}

TEST(SchedulerFactory, CreatedRRSchedulerWorks) {
    auto s = SchedulerFactory::create("rr");
    ASSERT_NE(s, nullptr);
    UEContext ue = makeUe(0x0001, 7);
    std::vector<UEContext*> ues{&ue};
    ResourceGrid grid{10};
    EXPECT_NO_THROW(s->schedule(ues, grid, 1));
    EXPECT_EQ(grid.freePrbCount(), 0u);
}

TEST(SchedulerFactory, CreatedPFSchedulerWorks) {
    auto s = SchedulerFactory::create("pf");
    ASSERT_NE(s, nullptr);
    UEContext ue = makeUe(0x0001, 12);
    std::vector<UEContext*> ues{&ue};
    ResourceGrid grid{10};
    EXPECT_NO_THROW(s->schedule(ues, grid, 1));
    EXPECT_EQ(grid.freePrbCount(), 0u);
}

// ══════════════════════════════════════════════════════════════════
// HARQManager + Scheduler Interaction
// ══════════════════════════════════════════════════════════════════
// In a real gNodeB, the MACLayer (Milestone 4) checks for NACK'd HARQ
// processes *before* calling the scheduler.  Retransmissions are given
// priority over new data.  This test demonstrates the three M2 classes
// (UEContext, ResourceGrid, HARQManager) being used alongside the
// scheduler in the same scheduling loop.

TEST(HarqSchedulerInteraction, SchedulerAndHarqManagerWorkTogether) {
    UEContext    ue{0x0001};
    ue.setCqi(9);
    HARQManager  harq;
    ResourceGrid grid{10};

    ProportionalFairScheduler pf;
    std::vector<UEContext*> ues{&ue};

    // ── TTI 1: new transmission ───────────────────────────────────
    // 1. Find an idle HARQ process.
    auto pid = harq.findIdleProcess();
    ASSERT_TRUE(pid.has_value());

    // 2. Scheduler allocates PRBs.
    pf.schedule(ues, grid, 1);
    EXPECT_EQ(grid.freePrbCount(), 0u);

    // 3. Mark the HARQ process as active.
    EXPECT_TRUE(harq.startTransmission(pid.value()));
    EXPECT_EQ(harq.state(pid.value()), HarqProcessState::WaitingAck);

    // ── TTI 2: NACK received → scheduler should prioritise retx ──
    harq.receiveNack(pid.value());
    EXPECT_TRUE(harq.hasNackedProcess());
    EXPECT_EQ(harq.state(pid.value()), HarqProcessState::Nacked);

    // Retransmit on the same process (would normally happen *before*
    // calling the scheduler for new data in M4 MACLayer).
    EXPECT_TRUE(harq.retransmit(pid.value()));
    EXPECT_EQ(harq.state(pid.value()), HarqProcessState::WaitingAck);
    EXPECT_EQ(harq.retxCount(pid.value()), 1u);

    // ── TTI 2: scheduler runs for any remaining UEs ───────────────
    grid.reset();
    pf.schedule(ues, grid, 2);
    EXPECT_EQ(grid.freePrbCount(), 0u);

    // ── TTI 3: ACK received → HARQ process freed ─────────────────
    harq.receiveAck(pid.value());
    EXPECT_EQ(harq.state(pid.value()), HarqProcessState::Idle);
    EXPECT_FALSE(harq.hasNackedProcess());
}
