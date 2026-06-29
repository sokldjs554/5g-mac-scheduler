add_executable(unit_tests
    # M1 skeleton check
    test_sanity.cpp
    # M2 core layer
    test_ue_context.cpp
    test_resource_grid.cpp
    test_harq_manager.cpp
    # M3 scheduler layer
    test_rr_scheduler.cpp
    test_pf_scheduler.cpp
    # M4 simulation layer
    test_traffic_generator.cpp
    test_statistics.cpp
    test_simulation_engine.cpp
)

# mac_core's PUBLIC include dirs propagate here automatically.
target_link_libraries(unit_tests PRIVATE
    mac_core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests)
