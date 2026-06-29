#include <gtest/gtest.h>

#include "scheduler/RoundRobinScheduler.hpp"
#include "core/ResourceGrid.hpp"
#include "core/UEContext.hpp"

// ── Helpers ──────────────────────────────────────────────────────
static UEContext makeUe(Rnti rnti, uint8_t cqi = 7) {
    UEContext ue{rnti};
    ue.setCqi(cqi);
    return ue;
}

// Count PRBs assigned to a specific RNTI.
static int countPrbs(const ResourceGrid& g, Rnti rnti) {
    int n = 0;
    for (uint8_t i = 0; i < g.numPrbs(); ++i) {
        auto r = g.allocatedUe(i);
        if (r.has_value() && r.value() == rnti) ++n;
    }
    return n;
}

// ── Fixture ──────────────────────────────────────────────────────
class RRSchedulerTest : public ::testing::Test {
protected:
    static constexpr uint8_t kNumPrbs = 10;

    RoundRobinScheduler sched;
    ResourceGrid        grid{kNumPrbs};
};

// ── Edge cases ───────────────────────────────────────────────────
TEST_F(RRSchedulerTest, EmptyUeListIsNoOp) {
    std::vector<UEContext*> ues;
    EXPECT_NO_THROW(sched.schedule(ues, grid, 1));
    EXPECT_EQ(grid.freePrbCount(), kNumPrbs); // nothing allocated
}

// ── Single UE ────────────────────────────────────────────────────
TEST_F(RRSchedulerTest, SingleUeReceivesAllPrbs) {
    UEContext ue = makeUe(0x0001);
    std::vector<UEContext*> ues{&ue};

    sched.schedule(ues, grid, 1);

    EXPECT_EQ(grid.freePrbCount(), 0u);
    for (uint8_t i = 0; i < kNumPrbs; ++i) {
        ASSERT_TRUE(grid.allocatedUe(i).has_value());
        EXPECT_EQ(grid.allocatedUe(i).value(), 0x0001u);
    }
}

TEST_F(RRSchedulerTest, SingleUeSchedulingCountIncremented) {
    UEContext ue = makeUe(0x0001);
    std::vector<UEContext*> ues{&ue};

    sched.schedule(ues, grid, 5);
    EXPECT_EQ(ue.schedulingCount(), 1u);
    EXPECT_EQ(ue.lastScheduledTti().value_or(0u), 5u);
}

// ── Equal distribution ───────────────────────────────────────────
TEST_F(RRSchedulerTest, TwoUesGetEqualPrbs) {
    UEContext ue1 = makeUe(0x0001);
    UEContext ue2 = makeUe(0x0002);
    std::vector<UEContext*> ues{&ue1, &ue2};

    sched.schedule(ues, grid, 1);

    EXPECT_EQ(grid.freePrbCount(), 0u);
    EXPECT_EQ(countPrbs(grid, 0x0001), 5); // 10/2 = 5 each
    EXPECT_EQ(countPrbs(grid, 0x0002), 5);
}

TEST_F(RRSchedulerTest, UnevenSplitGivesExtraPrbToFirstInRotation) {
    // 3 UEs, 10 PRBs: first UE = 4, others = 3
    UEContext ue1 = makeUe(0x0001);
    UEContext ue2 = makeUe(0x0002);
    UEContext ue3 = makeUe(0x0003);
    std::vector<UEContext*> ues{&ue1, &ue2, &ue3};

    sched.schedule(ues, grid, 1);

    EXPECT_EQ(countPrbs(grid, 0x0001), 4); // base 3 + 1 extra
    EXPECT_EQ(countPrbs(grid, 0x0002), 3);
    EXPECT_EQ(countPrbs(grid, 0x0003), 3);
}

// ── Round-robin rotation ─────────────────────────────────────────
TEST_F(RRSchedulerTest, FirstPrbBlockRotatesEachTti) {
    UEContext ue1 = makeUe(0x0001);
    UEContext ue2 = makeUe(0x0002);
    UEContext ue3 = makeUe(0x0003);
    std::vector<UEContext*> ues{&ue1, &ue2, &ue3};

    // TTI 1 — starts from UE1
    sched.schedule(ues, grid, 1);
    ASSERT_TRUE(grid.allocatedUe(0).has_value());
    EXPECT_EQ(grid.allocatedUe(0).value(), 0x0001u);

    // TTI 2 — starts from UE2
    grid.reset();
    sched.schedule(ues, grid, 2);
    ASSERT_TRUE(grid.allocatedUe(0).has_value());
    EXPECT_EQ(grid.allocatedUe(0).value(), 0x0002u);

    // TTI 3 — starts from UE3
    grid.reset();
    sched.schedule(ues, grid, 3);
    ASSERT_TRUE(grid.allocatedUe(0).has_value());
    EXPECT_EQ(grid.allocatedUe(0).value(), 0x0003u);

    // TTI 4 — wraps back to UE1
    grid.reset();
    sched.schedule(ues, grid, 4);
    ASSERT_TRUE(grid.allocatedUe(0).has_value());
    EXPECT_EQ(grid.allocatedUe(0).value(), 0x0001u);
}

TEST_F(RRSchedulerTest, AllUesGetEqualSchedulingCountOverCycle) {
    UEContext ue1 = makeUe(0x0001);
    UEContext ue2 = makeUe(0x0002);
    UEContext ue3 = makeUe(0x0003);
    std::vector<UEContext*> ues{&ue1, &ue2, &ue3};

    // 3 complete rotations = 9 TTIs
    for (TtiNumber t = 1; t <= 9; ++t) {
        grid.reset();
        sched.schedule(ues, grid, t);
    }
    EXPECT_EQ(ue1.schedulingCount(), 3u);
    EXPECT_EQ(ue2.schedulingCount(), 3u);
    EXPECT_EQ(ue3.schedulingCount(), 3u);
}

// ── More UEs than PRBs ───────────────────────────────────────────
TEST_F(RRSchedulerTest, MoreUesThanPrbsSchedulesOnlySubset) {
    // 6 UEs, 4 PRBs: first 4 (in rotation) receive 1 PRB each, last 2 skipped
    UEContext u1 = makeUe(0x0001); UEContext u2 = makeUe(0x0002);
    UEContext u3 = makeUe(0x0003); UEContext u4 = makeUe(0x0004);
    UEContext u5 = makeUe(0x0005); UEContext u6 = makeUe(0x0006);

    ResourceGrid smallGrid{4};
    std::vector<UEContext*> ues{&u1, &u2, &u3, &u4, &u5, &u6};

    sched.schedule(ues, smallGrid, 1);

    EXPECT_EQ(smallGrid.freePrbCount(), 0u);
    EXPECT_EQ(u1.schedulingCount(), 1u);
    EXPECT_EQ(u2.schedulingCount(), 1u);
    EXPECT_EQ(u3.schedulingCount(), 1u);
    EXPECT_EQ(u4.schedulingCount(), 1u);
    EXPECT_EQ(u5.schedulingCount(), 0u); // skipped
    EXPECT_EQ(u6.schedulingCount(), 0u); // skipped
}

TEST_F(RRSchedulerTest, SkippedUeHasZeroAvgThroughputDecay) {
    // 3 UEs, 2 PRBs: UE3 is skipped
    UEContext u1 = makeUe(0x0001, 10);
    UEContext u2 = makeUe(0x0002, 10);
    UEContext u3 = makeUe(0x0003, 10);

    ResourceGrid smallGrid{2};
    std::vector<UEContext*> ues{&u1, &u2, &u3};

    sched.schedule(ues, smallGrid, 1);

    EXPECT_GT(u1.avgThroughput(), 0.0); // received PRBs
    EXPECT_GT(u2.avgThroughput(), 0.0);
    EXPECT_DOUBLE_EQ(u3.avgThroughput(), 0.0); // skipped: updateAvgThroughput(0) called
}

// ── Throughput update ────────────────────────────────────────────
TEST_F(RRSchedulerTest, AvgThroughputUpdatedProportionalToCqi) {
    // With CQI 14 vs CQI 7: higher CQI earns more bits per PRB
    UEContext ueHigh = makeUe(0x0001, 14); // kBitsPerPrb[14] = 818
    UEContext ueLow  = makeUe(0x0002,  7); // kBitsPerPrb[ 7] = 236
    std::vector<UEContext*> ues{&ueHigh, &ueLow};

    sched.schedule(ues, grid, 1); // both get 5 PRBs

    // RR gives equal PRBs → higher CQI = higher throughput update
    EXPECT_GT(ueHigh.avgThroughput(), ueLow.avgThroughput());
}

TEST_F(RRSchedulerTest, IgnoresCqiWhenAllocatingPrbs) {
    // RR gives the same number of PRBs regardless of CQI
    UEContext ueHigh = makeUe(0x0001, 15);
    UEContext ueLow  = makeUe(0x0002,  1);
    std::vector<UEContext*> ues{&ueHigh, &ueLow};

    sched.schedule(ues, grid, 1);

    EXPECT_EQ(countPrbs(grid, 0x0001), 5);
    EXPECT_EQ(countPrbs(grid, 0x0002), 5);
}
