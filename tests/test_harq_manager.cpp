#include <gtest/gtest.h>
#include "core/HARQManager.hpp"

// ── Fixture ──────────────────────────────────────────────────────
class HARQManagerTest : public ::testing::Test {
protected:
    HARQManager harq;
};

// ── Construction ─────────────────────────────────────────────────
TEST_F(HARQManagerTest, AllProcessesIdleOnConstruction) {
    for (uint8_t i = 0; i < HARQManager::kNumProcesses; ++i) {
        EXPECT_EQ(harq.state(i), HarqProcessState::Idle)
            << "Process " << static_cast<int>(i) << " should be Idle";
        EXPECT_EQ(harq.retxCount(i), 0u);
    }
}

TEST_F(HARQManagerTest, NoNackedProcessInitially) {
    EXPECT_FALSE(harq.hasNackedProcess());
}

// ── findIdleProcess() ─────────────────────────────────────────────
TEST_F(HARQManagerTest, FindIdleProcessReturnsLowestIndexWhenAllFree) {
    auto pid = harq.findIdleProcess();
    ASSERT_TRUE(pid.has_value());
    EXPECT_EQ(pid.value(), 0u);
}

TEST_F(HARQManagerTest, FindIdleProcessSkipsBusyProcesses) {
    harq.startTransmission(0); // PID 0 is now WaitingAck
    auto pid = harq.findIdleProcess();
    ASSERT_TRUE(pid.has_value());
    EXPECT_EQ(pid.value(), 1u);
}

TEST_F(HARQManagerTest, FindIdleProcessReturnsNulloptWhenAllBusy) {
    for (uint8_t i = 0; i < HARQManager::kNumProcesses; ++i) {
        harq.startTransmission(i);
    }
    EXPECT_FALSE(harq.findIdleProcess().has_value());
}

// ── startTransmission() ──────────────────────────────────────────
TEST_F(HARQManagerTest, StartTransmissionMovesIdleToWaitingAck) {
    EXPECT_TRUE(harq.startTransmission(0));
    EXPECT_EQ(harq.state(0), HarqProcessState::WaitingAck);
    EXPECT_EQ(harq.retxCount(0), 0u);
}

TEST_F(HARQManagerTest, StartTransmissionOnBusyProcessFails) {
    harq.startTransmission(0);
    EXPECT_FALSE(harq.startTransmission(0));
    EXPECT_EQ(harq.state(0), HarqProcessState::WaitingAck); // unchanged
}

TEST_F(HARQManagerTest, StartTransmissionWithInvalidPidFails) {
    EXPECT_FALSE(harq.startTransmission(HARQManager::kNumProcesses));
}

// ── receiveAck() ─────────────────────────────────────────────────
TEST_F(HARQManagerTest, AckResetsProcessToIdle) {
    harq.startTransmission(0);
    harq.receiveAck(0);
    EXPECT_EQ(harq.state(0), HarqProcessState::Idle);
    EXPECT_EQ(harq.retxCount(0), 0u);
}

TEST_F(HARQManagerTest, AckMakesPidAvailableAgain) {
    harq.startTransmission(0);
    harq.receiveAck(0);
    // PID 0 should now be idle and usable again.
    EXPECT_TRUE(harq.startTransmission(0));
}

// ── receiveNack() ────────────────────────────────────────────────
TEST_F(HARQManagerTest, NackMovesWaitingAckToNacked) {
    harq.startTransmission(0);
    harq.receiveNack(0);
    EXPECT_EQ(harq.state(0), HarqProcessState::Nacked);
}

TEST_F(HARQManagerTest, NackOnIdleProcessIsIgnored) {
    harq.receiveNack(0); // process is Idle — must be a no-op
    EXPECT_EQ(harq.state(0), HarqProcessState::Idle);
}

TEST_F(HARQManagerTest, HasNackedProcessReturnsTrueAfterNack) {
    harq.startTransmission(0);
    harq.receiveNack(0);
    EXPECT_TRUE(harq.hasNackedProcess());
}

// ── retransmit() ─────────────────────────────────────────────────
TEST_F(HARQManagerTest, RetransmitMovesNackedToWaitingAck) {
    harq.startTransmission(0);
    harq.receiveNack(0);
    EXPECT_TRUE(harq.retransmit(0));
    EXPECT_EQ(harq.state(0), HarqProcessState::WaitingAck);
    EXPECT_EQ(harq.retxCount(0), 1u);
}

TEST_F(HARQManagerTest, RetransmitOnNonNackedProcessFails) {
    harq.startTransmission(0);
    EXPECT_FALSE(harq.retransmit(0)); // still WaitingAck
}

TEST_F(HARQManagerTest, MaxRetransmissionsAllSucceed) {
    harq.startTransmission(0);
    for (uint8_t i = 1; i <= HARQManager::kMaxRetransmissions; ++i) {
        harq.receiveNack(0);
        EXPECT_TRUE(harq.retransmit(0)) << "retx #" << static_cast<int>(i) << " should succeed";
        EXPECT_EQ(harq.retxCount(0), i);
    }
}

TEST_F(HARQManagerTest, ExceedingMaxRetransmissionsFlushesProcess) {
    harq.startTransmission(0);
    // Perform exactly kMaxRetransmissions retransmissions (should all succeed).
    for (uint8_t i = 0; i < HARQManager::kMaxRetransmissions; ++i) {
        harq.receiveNack(0);
        harq.retransmit(0);
    }
    // One more NACK + retransmit: must fail and flush.
    harq.receiveNack(0);
    EXPECT_FALSE(harq.retransmit(0));
    EXPECT_EQ(harq.state(0), HarqProcessState::Idle);
    EXPECT_EQ(harq.retxCount(0), 0u);
}

TEST_F(HARQManagerTest, FlushedProcessIsAvailableForNewTransmission) {
    harq.startTransmission(0);
    for (uint8_t i = 0; i < HARQManager::kMaxRetransmissions; ++i) {
        harq.receiveNack(0);
        harq.retransmit(0);
    }
    harq.receiveNack(0);
    harq.retransmit(0); // flushes

    EXPECT_TRUE(harq.startTransmission(0)); // must be usable again
}

// ── Edge cases ────────────────────────────────────────────────────
TEST_F(HARQManagerTest, InvalidPidIsHandledGracefully) {
    constexpr uint8_t kBad = HARQManager::kNumProcesses;
    EXPECT_FALSE(harq.startTransmission(kBad));
    EXPECT_EQ(harq.state(kBad), HarqProcessState::Idle); // safe default
    EXPECT_EQ(harq.retxCount(kBad), 0u);
    // None of these should crash or corrupt internal state.
    harq.receiveAck(kBad);
    harq.receiveNack(kBad);
    EXPECT_FALSE(harq.retransmit(kBad));
}
