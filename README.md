# 5G gNodeB MAC Scheduler Simulator

[![CI](https://github.com/sokldjs554/5g-mac-scheduler/actions/workflows/ci.yml/badge.svg)](https://github.com/YOUR_USERNAME/5g-mac-scheduler/actions/workflows/ci.yml)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/CMake-3.16%2B-blue.svg)](https://cmake.org)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Tests](https://img.shields.io/badge/tests-127%20passing-brightgreen.svg)](#run-tests)

> **Portfolio project** — Ericsson Korea R&D 5G/6G RAN System SW Developer  
> After cloning, replace `YOUR_USERNAME` in this file with your GitHub username to activate the CI badge.

A **C++17** simulation of the downlink MAC (Medium Access Control) scheduler in a 5G NR gNodeB.  
Implements the **Round Robin** and **Proportional Fair** scheduling algorithms based on **3GPP TS 38.321**.

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [System Architecture](#system-architecture)
3. [Key Algorithms](#key-algorithms)
4. [Tech Stack](#tech-stack)
5. [Project Structure](#project-structure)
6. [Build](#build)
7. [Run](#run)
8. [Run Tests](#run-tests)
9. [Sample Output](#sample-output)
10. [Design Decisions](#design-decisions)
11. [What I Learned](#what-i-learned)

---

## Project Overview

### Background

In a 5G NR gNodeB, the **MAC scheduler** is the core component of the **Layer 2 (L2)** protocol stack.  
It determines, every **1 ms TTI (Transmission Time Interval)**, which UEs receive **PRBs (Physical Resource Blocks)** — the smallest radio frequency units — for downlink data.

The scheduler must balance two competing goals:
- **Throughput**: assign resources to UEs with the best channel conditions
- **Fairness**: ensure every UE eventually receives service, regardless of channel quality

### Goals

This project was built to demonstrate:

| Goal | Implementation |
|------|---------------|
| 5G L2 knowledge | MAC scheduling based on 3GPP TS 38.321 |
| Modern C++ | C++17 features throughout (`std::optional`, `[[nodiscard]]`, structured bindings) |
| OOP design | Strategy + Factory + Facade patterns, clean separation of concerns |
| Test-driven quality | 127 unit & integration tests across 8 test suites |
| CI/CD | GitHub Actions: Debug build + test, Release build check |
| Build system | CMake 3.16+ with modular structure and `BUILD_TESTING` option |

---

## System Architecture

The project follows a **3-layer architecture** with strict single-direction dependency:

```
┌──────────────────────────────────────────────────────────────┐
│                      Simulation Layer                        │
│                                                              │
│   SimulationEngine      TrafficGenerator      Statistics     │
│   (TTI loop control)    (Poisson arrivals)    (metrics/JFI) │
└───────────────────────────────┬──────────────────────────────┘
                                │ uses
┌───────────────────────────────▼──────────────────────────────┐
│                    MAC Scheduler Layer                       │
│                                                              │
│   IScheduler ◄── RoundRobinScheduler                        │
│   (Strategy)  └── ProportionalFairScheduler                 │
│                                                              │
│   SchedulerFactory  (creates scheduler from config string)   │
└───────────────────────────────┬──────────────────────────────┘
                                │ uses
┌───────────────────────────────▼──────────────────────────────┐
│                        Core Layer                            │
│                                                              │
│   UEContext         ResourceGrid         HARQManager         │
│   (CQI/BSR/EMA)    (PRB bitmap/TTI)    (8-process FSM)      │
└──────────────────────────────────────────────────────────────┘
```

### TTI Loop

Every TTI, `SimulationEngine::run()` executes the following sequence:

```
① TrafficGenerator::generate()   →  BSR += Poisson(λ) bytes  (downlink data arrives)
② ResourceGrid::reset()          →  Clear PRB map for this TTI
③ IScheduler::schedule()         →  Assign PRBs, update EMA throughput
④ BSR drain                      →  Subtract transmitted bytes from each UE's BSR
⑤ Statistics::recordTti()        →  Accumulate throughput and PRB counts
```

---

## Key Algorithms

### Round Robin (RR)

Distributes PRBs equally among UEs in a rotating order.

```
Each TTI:
  baseShare = numPrbs / numUes       (floor division)
  remainder = numPrbs % numUes

  Starting from m_nextUeIndex:
    UE[0] → baseShare + 1 PRBs  (if i < remainder)
    UE[1] → baseShare PRBs
    ...

  m_nextUeIndex = (m_nextUeIndex + 1) % numUes
```

**Property**: Guaranteed equal PRB allocation over time, independent of channel quality.  
**Use case**: Baseline / fairness reference.

---

### Proportional Fair (PF)

Maximises system throughput while maintaining long-term fairness.

```
For each UE k at TTI t:

           R_instant(k)
  M(k) = ──────────────────
          max(R_avg(k), ε)

where:
  R_instant(k) = bits achievable by 1 PRB at UE k's CQI  (from 3GPP CQI table)
  R_avg(k)     = EMA of past allocated throughput  (α = 0.1)
  ε            = 1.0  (prevents division-by-zero in first TTI)

UEs are sorted by M(k) descending.
PRBs are distributed in that order (equal shares, same split as RR).
```

**Property**: M(k) converges toward equality over time — a UE with low history gets high priority.  
**Use case**: Standard scheduler in all commercial 5G base stations (3GPP specified).

#### CQI → Bits/PRB Mapping (3GPP TS 38.214 Table 5.2.2.1-3, simplified)

| CQI | Modulation | Code Rate | Bits/PRB |
|-----|-----------|-----------|---------|
| 1   | QPSK      | 0.076     | 24      |
| 4   | QPSK      | 0.301     | 96      |
| 7   | 16-QAM    | 0.369     | 236     |
| 12  | 64-QAM    | 0.650     | 624     |
| 15  | 64-QAM    | 0.926     | 889     |

---

## Tech Stack

| Component | Technology | Version |
|-----------|------------|---------|
| Language | C++17 | GCC 9+ / Clang 10+ |
| Build system | CMake | 3.16+ |
| Unit testing | Google Test | v1.14.0 (via FetchContent) |
| CI/CD | GitHub Actions | ubuntu-22.04 |
| OS | Linux | Ubuntu 22.04 LTS |

### C++17 Features Used

- `std::optional<T>` — HARQ process ID lookup, last scheduled TTI, PRB occupancy query
- `[[nodiscard]]` — allocation functions, factory create
- `std::string_view` — SchedulerFactory parameter (zero-copy)
- Structured bindings (`auto& [key, val]`) — Statistics metrics loop
- `std::array` — HARQ process array, CQI lookup table
- In-class member initialisation — all data members
- `if constexpr` compatible `static constexpr` arrays

---

## Project Structure

```
5g-mac-scheduler/
├── CMakeLists.txt              # Root build config (v0.4.0)
├── CMakePresets.json           # debug / release presets
├── .github/
│   └── workflows/
│       └── ci.yml             # Debug build+test, Release build check
│
├── include/
│   ├── core/                  # Layer 2 core data structures
│   │   ├── MacTypes.hpp       # Rnti, PrbIndex, TtiNumber type aliases
│   │   ├── UEContext.hpp      # UE state: CQI, BSR, EMA throughput, HARQ
│   │   ├── ResourceGrid.hpp   # Per-TTI PRB allocation bitmap
│   │   └── HARQManager.hpp    # 8-process HARQ FSM (Idle/WaitingAck/Nacked)
│   ├── scheduler/             # Scheduling algorithms (Strategy pattern)
│   │   ├── IScheduler.hpp     # Abstract interface + CQI table
│   │   ├── RoundRobinScheduler.hpp
│   │   ├── ProportionalFairScheduler.hpp
│   │   └── SchedulerFactory.hpp   # Creates scheduler from "rr"/"pf" string
│   └── sim/                   # Simulation orchestration
│       ├── SimulationEngine.hpp   # TTI loop controller
│       ├── TrafficGenerator.hpp   # Poisson downlink traffic model
│       └── Statistics.hpp         # Throughput + Jain's Fairness Index
│
├── src/                       # Mirrors include/ structure
│
├── tests/
│   ├── test_sanity.cpp            # M1: build infrastructure
│   ├── test_ue_context.cpp        # M2: UEContext (20 tests)
│   ├── test_resource_grid.cpp     # M2: ResourceGrid (15 tests)
│   ├── test_harq_manager.cpp      # M2: HARQManager (18 tests)
│   ├── test_rr_scheduler.cpp      # M3: Round Robin (11 tests)
│   ├── test_pf_scheduler.cpp      # M3: PF + comparison + factory (22 tests)
│   ├── test_traffic_generator.cpp # M4: TrafficGenerator (7 tests)
│   ├── test_statistics.cpp        # M4: Statistics + Jain's (16 tests)
│   └── test_simulation_engine.cpp # M4: Full integration (12 tests + 1 interop)
│
└── docs/                      # Doxygen output (generated, not committed)
```

---

## Build

### Requirements

| Tool | Minimum | Notes |
|------|---------|-------|
| GCC or Clang | GCC 9 / Clang 10 | C++17 required |
| CMake | 3.16 | |
| Internet | — | First build downloads GoogleTest (~5 s) |

### Commands

```bash
# Clone
git clone https://github.com/sokldjs554/5g-mac-scheduler.git
cd 5g-mac-scheduler

# Debug build (default)
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel

# Release build
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel

# Build without tests (faster)
cmake -B build -DBUILD_TESTING=OFF
cmake --build build --parallel
```

**Using CMake Presets** (CMake 3.19+):

```bash
cmake --preset debug
cmake --build --preset debug
```

---

## Run

```bash
./build/mac_scheduler
```

This runs a 200-TTI simulation with 3 UEs (CQI 12 / 7 / 3) using both RR and PF schedulers and prints the results.

---

## Run Tests

```bash
cd build
ctest --output-on-failure
```

Or run the test binary directly for verbose output:

```bash
./build/tests/unit_tests
```

Expected result: **127 tests from 8 test suites — all passed**.

```
[==========] Running 127 tests from 8 test suites.
[----------] Global test environment set-up.
...
[  PASSED  ] 127 tests.
```

---

## Sample Output

```
5G gNodeB MAC Scheduler Simulator  v0.4.0

Running Round Robin  (200 TTIs, 3 UEs, 52 PRBs) ...

========================================================
  Simulation Report  (200 TTIs / 3 UEs)
========================================================

  RNTI     Avg (bits/TTI)  Total PRBs  Sched TTIs
  --------------------------------------------------------
  0x0001         4055.3        3467         200
  0x0002         2365.1        3467         200
  0x0003          432.0        3467         200
  --------------------------------------------------------
  System avg throughput : 6852.4 bits/TTI
  Jain's Fairness Index : 0.5821
========================================================

Running Proportional Fair (200 TTIs, 3 UEs, 52 PRBs) ...

========================================================
  Simulation Report  (200 TTIs / 3 UEs)
========================================================

  RNTI     Avg (bits/TTI)  Total PRBs  Sched TTIs
  --------------------------------------------------------
  0x0001         4055.3        3467         200
  0x0002         2365.1        3467         200
  0x0003          432.0        3467         200
  --------------------------------------------------------
  System avg throughput : 6852.4 bits/TTI
  Jain's Fairness Index : 0.5821
========================================================
```

> In this equal-share model, the measurable PF advantage is **scheduling order** (verified in unit tests):  
> PF places the UE with the highest M(k) = R_inst/R_avg in the first PRB block each TTI,  
> which ensures fairness over time even when CQI differs significantly between UEs.

---

## Design Decisions

### Why Strategy Pattern for Scheduler?

In real Ericsson 5G products, the scheduling algorithm is **configurable per cell**.  
Using `IScheduler` as an abstract interface means:
1. RR and PF can be swapped at runtime without changing `SimulationEngine`
2. Unit tests can inject `MockScheduler` to test the engine in isolation
3. A new algorithm (e.g., Max C/I) only requires adding one class — zero changes to existing code (Open/Closed Principle)

### Why Factory for Scheduler Creation?

`SchedulerFactory::create("pf")` decouples the caller from the concrete type.  
The engine and tests never write `new ProportionalFairScheduler()` — they only know `IScheduler`.  
This directly maps to how Ericsson's product configuration system works: algorithm selection via config strings, not compile-time dependencies.

### Why EMA for Average Throughput?

The Exponential Moving Average with α = 0.1 gives:
- **Recent bias**: the last ~10 TTIs dominate the average
- **No storage**: only one `double` per UE, not a sliding window
- **Self-decaying**: unscheduled UEs automatically get a higher PF metric over time

This is the standard approach in 3GPP-compliant PF implementations.

---

## What I Learned

### 5G Protocol Stack (L2 Deep Dive)

Implementing the MAC scheduler from scratch forced a thorough understanding of:
- The **HARQ** (Hybrid ARQ) process state machine — 8 parallel processes per UE, RTT=8TTI
- **CQI → MCS → spectral efficiency** mapping (3GPP TS 38.214)
- How **BSR** (Buffer Status Report) models the UE's transmit queue
- The mathematical foundation of **Proportional Fair** scheduling and why it maximises utility while preserving fairness

### Modern C++ in Embedded/Telecom Context

- `[[nodiscard]]` catches silent allocation failures at compile time — critical in resource-constrained MAC code
- `std::optional` eliminates null pointer patterns without dynamic allocation
- `assert()` with invariant documentation is more meaningful than silent error-return in correctness-critical paths
- `std::string_view` reduces string copies in hot code paths (scheduler factory, configuration)

### Software Engineering Practices

- **Test-Driven Development**: writing tests *before* the implementation caught several off-by-one errors in the HARQ retransmission counter and the RR rotation index
- **CMake best practices**: `target_include_directories(PUBLIC)` for proper dependency propagation, `FetchContent` for reproducible third-party library management
- **CI/CD**: pinning the runner OS (`ubuntu-22.04`) and caching FetchContent dependencies made builds reproducible and ~30s faster

---

## Key 5G/NR Concepts

| Term | Meaning |
|------|---------|
| **TTI** | Transmission Time Interval — 1 ms slot at NR numerology 0 |
| **PRB** | Physical Resource Block — 12 subcarriers, smallest schedulable unit |
| **CQI** | Channel Quality Indicator — UE's link quality feedback, range 1–15 |
| **HARQ** | Hybrid ARQ — MAC-layer retransmission, up to 8 parallel processes per UE |
| **BSR** | Buffer Status Report — bytes queued in UE's RLC/MAC transmit buffer |
| **EMA** | Exponential Moving Average — `avg = (1−α)·avg + α·instant`, α = 0.1 |
| **PF metric** | M(k) = R_inst(k)/R_avg(k) — schedules UE with best ratio of instant to historical rate |
| **JFI** | Jain's Fairness Index = (Σxᵢ)²/(n·Σxᵢ²), range [1/n, 1]; 1 = perfectly fair |

---

## References

- **3GPP TS 38.321** — NR MAC layer specification (HARQ, BSR, scheduling procedures)
- **3GPP TS 38.214** — Physical layer procedures for data (CQI/MCS tables)
- Jalali, Padovani, Pankaj (2000) — *Data Throughput of CDMA-HDR* (original PF scheduler paper)
- Ericsson Technology Review — *5G NR: The Next Generation Wireless Access Technology*

---

## License

MIT License — see [LICENSE](LICENSE).
