#include <gtest/gtest.h>

#include "sim/Statistics.hpp"
#include "core/ResourceGrid.hpp"
#include "core/UEContext.hpp"

// ── Helper ────────────────────────────────────────────────────────────────────
// Create a UEContext whose lastTtiThroughput() returns `bits`.
// updateAvgThroughput() stores the instant value in m_lastTtiThroughput.
static UEContext makeUeWithThroughput(Rnti rnti, double bits) {
    UEContext ue{rnti};
    ue.updateAvgThroughput(bits);
    return ue;
}

// ── Fixture ───────────────────────────────────────────────────────────────────
class StatisticsTest : public ::testing::Test {
protected:
    Statistics   stats;
    ResourceGrid emptyGrid{52};   // no PRBs allocated — used when PRB counts don't matter
};

// ── Initial state ─────────────────────────────────────────────────────────────

TEST_F(StatisticsTest, InitiallyEmpty) {
    EXPECT_EQ(stats.numTtisRecorded(), 0u);
    EXPECT_DOUBLE_EQ(stats.totalThroughputBits(), 0.0);
    EXPECT_DOUBLE_EQ(stats.avgSystemThroughputPerTti(), 0.0);
    EXPECT_NEAR(stats.jainsFairnessIndex(), 1.0, 1e-9); // 0/0 → 1 by convention
}

TEST_F(StatisticsTest, UnknownRntiReturnsZeroThroughput) {
    EXPECT_DOUBLE_EQ(stats.avgThroughputForUe(0xFFFF), 0.0);
}

// ── Single TTI ────────────────────────────────────────────────────────────────

TEST_F(StatisticsTest, RecordsOneTtiCorrectly) {
    UEContext ue = makeUeWithThroughput(0x0001, 1000.0);
    std::vector<UEContext*> ues{&ue};

    stats.recordTti(ues, emptyGrid, 1);

    EXPECT_EQ(stats.numTtisRecorded(), 1u);
    EXPECT_NEAR(stats.totalThroughputBits(), 1000.0, 1e-9);
    EXPECT_NEAR(stats.avgSystemThroughputPerTti(), 1000.0, 1e-9);
}

TEST_F(StatisticsTest, AvgThroughputForUeCorrect) {
    UEContext ue = makeUeWithThroughput(0x0001, 2000.0);
    std::vector<UEContext*> ues{&ue};

    stats.recordTti(ues, emptyGrid, 1);

    EXPECT_NEAR(stats.avgThroughputForUe(0x0001), 2000.0, 1e-9);
}

// ── Accumulation across TTIs ──────────────────────────────────────────────────

TEST_F(StatisticsTest, CumulatesOverMultipleTtis) {
    UEContext ue = makeUeWithThroughput(0x0001, 1000.0);
    std::vector<UEContext*> ues{&ue};

    for (TtiNumber t = 1; t <= 5; ++t) {
        stats.recordTti(ues, emptyGrid, t);
    }

    EXPECT_EQ(stats.numTtisRecorded(), 5u);
    EXPECT_NEAR(stats.totalThroughputBits(), 5000.0, 1e-9);
    EXPECT_NEAR(stats.avgThroughputForUe(0x0001), 1000.0, 1e-9);
}

TEST_F(StatisticsTest, AvgThroughputAveragedOverTtis) {
    UEContext ue1 = makeUeWithThroughput(0x0001, 2000.0);
    std::vector<UEContext*> ues1{&ue1};
    stats.recordTti(ues1, emptyGrid, 1);   // 2000 bits

    UEContext ue2 = makeUeWithThroughput(0x0001, 4000.0);
    std::vector<UEContext*> ues2{&ue2};
    stats.recordTti(ues2, emptyGrid, 2);   // 4000 bits

    // Average over 2 TTIs = (2000 + 4000) / 2 = 3000
    EXPECT_NEAR(stats.avgThroughputForUe(0x0001), 3000.0, 1e-9);
}

// ── Jain's Fairness Index ─────────────────────────────────────────────────────

TEST_F(StatisticsTest, JainsSingleUeIsOne) {
    // J = x² / (1·x²) = 1.0 for any x > 0
    UEContext ue = makeUeWithThroughput(0x0001, 500.0);
    std::vector<UEContext*> ues{&ue};
    stats.recordTti(ues, emptyGrid, 1);
    EXPECT_NEAR(stats.jainsFairnessIndex(), 1.0, 1e-9);
}

TEST_F(StatisticsTest, JainsEqualTwoUesIsOne) {
    // x1 = x2 = 1000  →  J = (2000)² / (2 × 2×10⁶) = 4×10⁶ / 4×10⁶ = 1.0
    UEContext u1 = makeUeWithThroughput(0x0001, 1000.0);
    UEContext u2 = makeUeWithThroughput(0x0002, 1000.0);
    std::vector<UEContext*> ues{&u1, &u2};
    stats.recordTti(ues, emptyGrid, 1);
    EXPECT_NEAR(stats.jainsFairnessIndex(), 1.0, 1e-9);
}

TEST_F(StatisticsTest, JainsEqualThreeUesIsOne) {
    UEContext u1 = makeUeWithThroughput(0x0001, 500.0);
    UEContext u2 = makeUeWithThroughput(0x0002, 500.0);
    UEContext u3 = makeUeWithThroughput(0x0003, 500.0);
    std::vector<UEContext*> ues{&u1, &u2, &u3};
    stats.recordTti(ues, emptyGrid, 1);
    EXPECT_NEAR(stats.jainsFairnessIndex(), 1.0, 1e-9);
}

TEST_F(StatisticsTest, JainsTwoUesOneGetsEverythingIsHalf) {
    // x1 = 1000, x2 = 0  →  J = 1000² / (2 × 1000²) = 0.5
    UEContext u1 = makeUeWithThroughput(0x0001, 1000.0);
    UEContext u2 = makeUeWithThroughput(0x0002, 0.0);
    std::vector<UEContext*> ues{&u1, &u2};
    stats.recordTti(ues, emptyGrid, 1);
    EXPECT_NEAR(stats.jainsFairnessIndex(), 0.5, 1e-9);
}

TEST_F(StatisticsTest, JainsThreeUesOneGetsEverythingIsOneThird) {
    // x1 = T, x2 = x3 = 0  →  J = T² / (3·T²) = 1/3
    UEContext u1 = makeUeWithThroughput(0x0001, 900.0);
    UEContext u2 = makeUeWithThroughput(0x0002, 0.0);
    UEContext u3 = makeUeWithThroughput(0x0003, 0.0);
    std::vector<UEContext*> ues{&u1, &u2, &u3};
    stats.recordTti(ues, emptyGrid, 1);
    EXPECT_NEAR(stats.jainsFairnessIndex(), 1.0 / 3.0, 1e-9);
}

TEST_F(StatisticsTest, JainsAlwaysInBounds) {
    UEContext u1 = makeUeWithThroughput(0x0001, 1234.5);
    UEContext u2 = makeUeWithThroughput(0x0002,  300.0);
    UEContext u3 = makeUeWithThroughput(0x0003,   42.0);
    std::vector<UEContext*> ues{&u1, &u2, &u3};
    stats.recordTti(ues, emptyGrid, 1);

    const double jfi = stats.jainsFairnessIndex();
    EXPECT_GE(jfi, 0.0);
    EXPECT_LE(jfi, 1.0 + 1e-9);
}

// ── PRB counting from grid ─────────────────────────────────────────────────────

TEST_F(StatisticsTest, CountsPrbsFromAllocatedGrid) {
    UEContext ue = makeUeWithThroughput(0x0001, 500.0);
    std::vector<UEContext*> ues{&ue};

    // Allocate 6 PRBs to the UE in the grid
    ResourceGrid allocGrid{10};
    allocGrid.allocateRange(0x0001, 0, 6);

    stats.recordTti(ues, allocGrid, 1);

    // Verify the record was created (system throughput > 0)
    EXPECT_NEAR(stats.totalThroughputBits(), 500.0, 1e-9);
    EXPECT_EQ(stats.numTtisRecorded(), 1u);
}

TEST_F(StatisticsTest, MultipleUesPrbsSeparatedCorrectly) {
    UEContext u1 = makeUeWithThroughput(0x0001, 300.0);
    UEContext u2 = makeUeWithThroughput(0x0002, 200.0);
    std::vector<UEContext*> ues{&u1, &u2};

    ResourceGrid grid{10};
    grid.allocateRange(0x0001, 0, 4);
    grid.allocateRange(0x0002, 4, 6);

    stats.recordTti(ues, grid, 1);

    EXPECT_NEAR(stats.totalThroughputBits(), 500.0, 1e-9);
}

// ── printReport ────────────────────────────────────────────────────────────────

TEST_F(StatisticsTest, PrintReportOnEmptyStatsDoesNotCrash) {
    // Empty statistics must produce some output without crashing.
    EXPECT_NO_THROW(stats.printReport());
}

TEST_F(StatisticsTest, PrintReportOnPopulatedStatsDoesNotCrash) {
    UEContext u1 = makeUeWithThroughput(0x0001, 1000.0);
    UEContext u2 = makeUeWithThroughput(0x0002,  500.0);
    std::vector<UEContext*> ues{&u1, &u2};

    stats.recordTti(ues, emptyGrid, 1);
    stats.recordTti(ues, emptyGrid, 2);

    EXPECT_NO_THROW(stats.printReport());
}
