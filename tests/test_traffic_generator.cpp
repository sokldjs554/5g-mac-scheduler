#include <gtest/gtest.h>

#include "sim/TrafficGenerator.hpp"
#include "core/UEContext.hpp"

// ── Edge cases ────────────────────────────────────────────────────────────────

TEST(TrafficGeneratorTest, DefaultConstruction) {
    EXPECT_NO_THROW(TrafficGenerator gen);
}

TEST(TrafficGeneratorTest, EmptyUeListIsNoOp) {
    TrafficGenerator gen{1000.0, 42};
    std::vector<UEContext*> empty;
    EXPECT_NO_THROW(gen.generate(empty));
}

// ── BSR update ────────────────────────────────────────────────────────────────

TEST(TrafficGeneratorTest, IncreasesUeBsr) {
    // λ = 10 000 bits/TTI → ~1 250 bytes/TTI on average.
    // The probability of drawing exactly 0 arrivals is e^(-10000) ≈ 0, so
    // BSR must increase after one call.
    UEContext ue{0x0001}; ue.setCqi(7);
    std::vector<UEContext*> ues{&ue};

    TrafficGenerator gen{10000.0, 42};
    gen.generate(ues);

    EXPECT_GT(ue.bsr(), 0u);
}

TEST(TrafficGeneratorTest, BsrAccumulatesAcrossTtis) {
    UEContext ue{0x0001}; ue.setCqi(7);
    std::vector<UEContext*> ues{&ue};

    TrafficGenerator gen{1000.0, 42};
    gen.generate(ues);
    const uint32_t afterOne = ue.bsr();

    gen.generate(ues);
    EXPECT_GE(ue.bsr(), afterOne);   // BSR can only grow via traffic
}

TEST(TrafficGeneratorTest, IndependentlyUpdatesEachUe) {
    UEContext u1{0x0001}; u1.setCqi(7);
    UEContext u2{0x0002}; u2.setCqi(7);
    std::vector<UEContext*> ues{&u1, &u2};

    TrafficGenerator gen{10000.0, 42};
    gen.generate(ues);

    EXPECT_GT(u1.bsr(), 0u);
    EXPECT_GT(u2.bsr(), 0u);
}

// ── Reproducibility ───────────────────────────────────────────────────────────

TEST(TrafficGeneratorTest, SameSeedProducesIdenticalResults) {
    UEContext ua{0x0001}; ua.setCqi(7);
    UEContext ub{0x0001}; ub.setCqi(7);

    TrafficGenerator ga{1000.0, 42};
    TrafficGenerator gb{1000.0, 42};

    std::vector<UEContext*> usa{&ua};
    std::vector<UEContext*> usb{&ub};

    // Run 10 TTIs — both sequences must be identical
    for (int i = 0; i < 10; ++i) {
        ga.generate(usa);
        gb.generate(usb);
    }
    EXPECT_EQ(ua.bsr(), ub.bsr());
}

TEST(TrafficGeneratorTest, DifferentSeedsProduceDifferentResults) {
    UEContext ua{0x0001}; ua.setCqi(7);
    UEContext ub{0x0001}; ub.setCqi(7);

    TrafficGenerator ga{1000.0, 42};
    TrafficGenerator gb{1000.0, 99};   // different seed

    std::vector<UEContext*> usa{&ua};
    std::vector<UEContext*> usb{&ub};

    for (int i = 0; i < 20; ++i) {
        ga.generate(usa);
        gb.generate(usb);
    }
    // Probability of 20 identical draws from two different sequences ≈ 0
    EXPECT_NE(ua.bsr(), ub.bsr());
}

// ── Accessor ─────────────────────────────────────────────────────────────────

TEST(TrafficGeneratorTest, ArrivalRateAccessorReturnsConfiguredValue) {
    TrafficGenerator gen{750.0, 42};
    EXPECT_DOUBLE_EQ(gen.arrivalRateBitsPerTti(), 750.0);
}
