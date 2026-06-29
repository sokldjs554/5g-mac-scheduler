#include <gtest/gtest.h>
#include "core/UEContext.hpp"

// ── Fixture ──────────────────────────────────────────────────────
class UEContextTest : public ::testing::Test {
protected:
    static constexpr Rnti kRnti = 0x1001;
    UEContext ue{kRnti};
};

// ── Construction ─────────────────────────────────────────────────
TEST_F(UEContextTest, ConstructorSetsRnti) {
    EXPECT_EQ(ue.rnti(), kRnti);
}

TEST_F(UEContextTest, DefaultCqiIsMidRange) {
    EXPECT_EQ(ue.cqi(), UEContext::kDefaultCqi);
}

TEST_F(UEContextTest, DefaultBsrIsZero) {
    EXPECT_EQ(ue.bsr(), 0u);
}

TEST_F(UEContextTest, DefaultAvgThroughputIsZero) {
    EXPECT_DOUBLE_EQ(ue.avgThroughput(), 0.0);
}

TEST_F(UEContextTest, DefaultSchedulingCountIsZero) {
    EXPECT_EQ(ue.schedulingCount(), 0u);
}

TEST_F(UEContextTest, NeverScheduledReturnsNullopt) {
    EXPECT_FALSE(ue.lastScheduledTti().has_value());
}

// ── CQI ──────────────────────────────────────────────────────────
TEST_F(UEContextTest, SetCqiValidBoundaryValues) {
    EXPECT_NO_THROW(ue.setCqi(UEContext::kMinCqi));
    EXPECT_EQ(ue.cqi(), UEContext::kMinCqi);

    EXPECT_NO_THROW(ue.setCqi(UEContext::kMaxCqi));
    EXPECT_EQ(ue.cqi(), UEContext::kMaxCqi);
}

TEST_F(UEContextTest, SetCqiAllValidValues) {
    for (uint8_t cqi = UEContext::kMinCqi; cqi <= UEContext::kMaxCqi; ++cqi) {
        EXPECT_NO_THROW(ue.setCqi(cqi));
        EXPECT_EQ(ue.cqi(), cqi);
    }
}

TEST_F(UEContextTest, SetCqiZeroThrows) {
    EXPECT_THROW(ue.setCqi(0), std::out_of_range);
}

TEST_F(UEContextTest, SetCqiAboveMaxThrows) {
    EXPECT_THROW(ue.setCqi(UEContext::kMaxCqi + 1), std::out_of_range);
}

TEST_F(UEContextTest, CqiUnchangedAfterThrow) {
    ue.setCqi(10);
    EXPECT_THROW(ue.setCqi(0), std::out_of_range);
    EXPECT_EQ(ue.cqi(), 10u); // must remain at the last valid value
}

// ── BSR ──────────────────────────────────────────────────────────
TEST_F(UEContextTest, SetBsrUpdatesValue) {
    ue.setBsr(4096);
    EXPECT_EQ(ue.bsr(), 4096u);
}

TEST_F(UEContextTest, SetBsrZeroIsValid) {
    ue.setBsr(1000);
    ue.setBsr(0);
    EXPECT_EQ(ue.bsr(), 0u);
}

// ── Average throughput (EMA) ──────────────────────────────────────
TEST_F(UEContextTest, UpdateAvgThroughputFirstCall) {
    // avg = (1-α)×0 + α×1000 = 0.1×1000 = 100
    ue.updateAvgThroughput(1000.0);
    EXPECT_NEAR(ue.avgThroughput(), 100.0, 1e-9);
}

TEST_F(UEContextTest, UpdateAvgThroughputSecondCall) {
    ue.updateAvgThroughput(1000.0); // avg = 100
    // avg = (1-0.1)×100 + 0.1×1000 = 90 + 100 = 190
    ue.updateAvgThroughput(1000.0);
    EXPECT_NEAR(ue.avgThroughput(), 190.0, 1e-9);
}

TEST_F(UEContextTest, UpdateAvgThroughputWithZeroDecays) {
    ue.updateAvgThroughput(1000.0); // avg = 100
    // avg = (1-0.1)×100 + 0.1×0 = 90
    ue.updateAvgThroughput(0.0);
    EXPECT_NEAR(ue.avgThroughput(), 90.0, 1e-9);
}

// ── Scheduling history ────────────────────────────────────────────
TEST_F(UEContextTest, RecordScheduledIncrementsCount) {
    ue.recordScheduled(1);
    EXPECT_EQ(ue.schedulingCount(), 1u);

    ue.recordScheduled(2);
    EXPECT_EQ(ue.schedulingCount(), 2u);
}

TEST_F(UEContextTest, RecordScheduledUpdatesLastTti) {
    ue.recordScheduled(42);
    ASSERT_TRUE(ue.lastScheduledTti().has_value());
    EXPECT_EQ(ue.lastScheduledTti().value(), 42u);
}

TEST_F(UEContextTest, RecordScheduledOverwritesLastTti) {
    ue.recordScheduled(10);
    ue.recordScheduled(20);
    ASSERT_TRUE(ue.lastScheduledTti().has_value());
    EXPECT_EQ(ue.lastScheduledTti().value(), 20u);
}

// ── lastTtiThroughput (set by updateAvgThroughput, read by Statistics) ────────
TEST_F(UEContextTest, DefaultLastTtiThroughputIsZero) {
    EXPECT_DOUBLE_EQ(ue.lastTtiThroughput(), 0.0);
}

TEST_F(UEContextTest, LastTtiThroughputTracksInstantBits) {
    ue.updateAvgThroughput(500.0);
    EXPECT_DOUBLE_EQ(ue.lastTtiThroughput(), 500.0);
}

TEST_F(UEContextTest, LastTtiThroughputUpdatedEachCall) {
    ue.updateAvgThroughput(1000.0);
    ue.updateAvgThroughput(0.0);   // not scheduled this TTI
    EXPECT_DOUBLE_EQ(ue.lastTtiThroughput(), 0.0);
}
