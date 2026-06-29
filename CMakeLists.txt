cmake_minimum_required(VERSION 3.16)

project(mac_scheduler
    VERSION     0.4.0
    DESCRIPTION "5G gNodeB MAC Scheduler Simulator"
    LANGUAGES   CXX
)

# ── Language standard ────────────────────────────────────────────
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

# ── Export compile_commands.json for clangd / IDE integration ────
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Default build type ──────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# ── Compiler warnings ───────────────────────────────────────────
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ── GoogleTest via FetchContent ──────────────────────────────────
# Suppress CMake 3.24+ DOWNLOAD_EXTRACT_TIMESTAMP warning.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
    cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)
# Prevent GTest from overriding compiler/linker settings on Windows.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ── mac_core library ─────────────────────────────────────────────
# PUBLIC include dirs propagate to every target that links mac_core.
add_library(mac_core
    # Core layer (M2)
    src/core/UEContext.cpp
    src/core/ResourceGrid.cpp
    src/core/HARQManager.cpp
    # Scheduler layer (M3)
    src/scheduler/RoundRobinScheduler.cpp
    src/scheduler/ProportionalFairScheduler.cpp
    src/scheduler/SchedulerFactory.cpp
    # Simulation layer (M4)
    src/sim/TrafficGenerator.cpp
    src/sim/Statistics.cpp
    src/sim/SimulationEngine.cpp
)
target_include_directories(mac_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# ── Simulator executable ────────────────────────────────────────
add_executable(mac_scheduler src/main.cpp)
target_link_libraries(mac_scheduler PRIVATE mac_core)

# ── Unit tests (can be disabled with -DBUILD_TESTING=OFF) ────────
option(BUILD_TESTING "Enable unit tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
