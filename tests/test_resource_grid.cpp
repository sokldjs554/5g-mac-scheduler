#include <gtest/gtest.h>
#include "core/ResourceGrid.hpp"

// ── Fixture ──────────────────────────────────────────────────────
class ResourceGridTest : public ::testing::Test {
protected:
    static constexpr uint8_t kPrbs = 10;
    static constexpr Rnti    kUe1  = 0x0001;
    static constexpr Rnti    kUe2  = 0x0002;

    ResourceGrid grid{kPrbs};
};

// ── Construction ─────────────────────────────────────────────────
TEST_F(ResourceGridTest, ConstructorSetsNumPrbs) {
    EXPECT_EQ(grid.numPrbs(), kPrbs);
}

TEST_F(ResourceGridTest, AllPrbsFreeAfterConstruction) {
    EXPECT_EQ(grid.freePrbCount(), kPrbs);
}

TEST_F(ResourceGridTest, ZeroSizeConstructorThrows) {
    EXPECT_THROW(ResourceGrid{0}, std::invalid_argument);
}

// ── reset() ──────────────────────────────────────────────────────
TEST_F(ResourceGridTest, ResetClearsAllAllocations) {
    grid.allocate(kUe1, 0);
    grid.allocate(kUe2, 1);
    grid.reset();
    EXPECT_EQ(grid.freePrbCount(), kPrbs);
    EXPECT_TRUE(grid.isFree(0));
    EXPECT_TRUE(grid.isFree(1));
}

// ── allocate() ───────────────────────────────────────────────────
TEST_F(ResourceGridTest, AllocateSinglePrbSucceeds) {
    EXPECT_TRUE(grid.allocate(kUe1, 0));
    EXPECT_FALSE(grid.isFree(0));
    EXPECT_EQ(grid.freePrbCount(), kPrbs - 1);
}

TEST_F(ResourceGridTest, AllocatedUeReturnsCorrectRnti) {
    grid.allocate(kUe1, 3);
    ASSERT_TRUE(grid.allocatedUe(3).has_value());
    EXPECT_EQ(grid.allocatedUe(3).value(), kUe1);
}

TEST_F(ResourceGridTest, FreePrbReturnsNullopt) {
    EXPECT_FALSE(grid.allocatedUe(5).has_value());
}

TEST_F(ResourceGridTest, DuplicateAllocationSamePrbFails) {
    EXPECT_TRUE(grid.allocate(kUe1, 0));
    EXPECT_FALSE(grid.allocate(kUe2, 0)); // same PRB, different UE
    // Original allocation must be preserved.
    ASSERT_TRUE(grid.allocatedUe(0).has_value());
    EXPECT_EQ(grid.allocatedUe(0).value(), kUe1);
}

TEST_F(ResourceGridTest, DuplicateAllocationSameUeSamePrbFails) {
    EXPECT_TRUE(grid.allocate(kUe1, 0));
    EXPECT_FALSE(grid.allocate(kUe1, 0)); // same UE, same PRB
}

TEST_F(ResourceGridTest, AllPrbsCanBeAllocated) {
    for (uint8_t i = 0; i < kPrbs; ++i) {
        EXPECT_TRUE(grid.allocate(kUe1, i));
    }
    EXPECT_EQ(grid.freePrbCount(), 0u);
}

TEST_F(ResourceGridTest, OutOfRangeIndexReturnsFalse) {
    EXPECT_FALSE(grid.allocate(kUe1, kPrbs));      // exact boundary
    EXPECT_FALSE(grid.allocate(kUe1, kPrbs + 5));  // well beyond
}

TEST_F(ResourceGridTest, IsFreeReturnsFalseForOutOfRange) {
    EXPECT_FALSE(grid.isFree(kPrbs));
}

TEST_F(ResourceGridTest, AllocatedUeReturnsNulloptForOutOfRange) {
    EXPECT_FALSE(grid.allocatedUe(kPrbs).has_value());
}

// ── allocateRange() ───────────────────────────────────────────────
TEST_F(ResourceGridTest, AllocateRangeSucceedsOnFreeGrid) {
    EXPECT_TRUE(grid.allocateRange(kUe1, 2, 4)); // PRBs 2,3,4,5
    EXPECT_EQ(grid.freePrbCount(), kPrbs - 4);
    for (uint8_t i = 2; i < 6; ++i) {
        EXPECT_FALSE(grid.isFree(i));
        ASSERT_TRUE(grid.allocatedUe(i).has_value());
        EXPECT_EQ(grid.allocatedUe(i).value(), kUe1);
    }
}

TEST_F(ResourceGridTest, AllocateRangeIsAtomic_FailsIfAnyPrbBusy) {
    grid.allocate(kUe1, 3); // PRB 3 is taken
    // Trying to allocate PRBs 0-4 must fail atomically.
    EXPECT_FALSE(grid.allocateRange(kUe2, 0, 5));
    // Only the original allocation at PRB 3 should remain.
    EXPECT_EQ(grid.freePrbCount(), kPrbs - 1);
    EXPECT_TRUE(grid.isFree(0));
    EXPECT_TRUE(grid.isFree(1));
    EXPECT_TRUE(grid.isFree(2));
}

TEST_F(ResourceGridTest, AllocateRangeFailsIfExceedsBoundary) {
    // start=8, count=5 → 8+5=13 > 10
    EXPECT_FALSE(grid.allocateRange(kUe1, 8, 5));
}

TEST_F(ResourceGridTest, AllocateRangeWithZeroCountFails) {
    EXPECT_FALSE(grid.allocateRange(kUe1, 0, 0));
}

// ── freePrbCount() ────────────────────────────────────────────────
TEST_F(ResourceGridTest, FreePrbCountDecrementsWithEachAllocation) {
    for (uint8_t i = 0; i < kPrbs; ++i) {
        EXPECT_EQ(grid.freePrbCount(), kPrbs - i);
        grid.allocate(kUe1, i);
    }
    EXPECT_EQ(grid.freePrbCount(), 0u);
}
