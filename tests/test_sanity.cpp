#include <gtest/gtest.h>

// ── Milestone 1: Build infrastructure sanity checks ──────────────
//
// These tests verify that the project skeleton is correctly set up:
//   1. The project builds without errors.
//   2. C++17 is enabled (required for <optional>, <variant>, etc.).
//
// Business-logic tests (UEContext, Scheduler, ...) will be added
// in Milestone 2 and beyond.

TEST(Milestone1, ProjectBuilds) {
    // If this test executes, cmake + make succeeded.
    SUCCEED();
}

TEST(Milestone1, CppStandardIs17OrNewer) {
    // C++17 sets __cplusplus to exactly 201703L.
    // Verify at compile time and at runtime.
    static_assert(__cplusplus >= 201703L, "C++17 is required");
    EXPECT_GE(__cplusplus, 201703L);
}
