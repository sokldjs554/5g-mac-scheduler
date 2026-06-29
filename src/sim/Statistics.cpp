#include "sim/Statistics.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unordered_map>

// ── recordTti ────────────────────────────────────────────────────────────────

void Statistics::recordTti(const std::vector<UEContext*>& ues,
                            const ResourceGrid&             grid,
                            TtiNumber                       /*tti*/)
{
    // Count PRBs allocated per UE by scanning the grid once (O(numPrbs)).
    std::unordered_map<Rnti, uint8_t> prbsPerUe;
    for (uint8_t i = 0; i < grid.numPrbs(); ++i) {
        auto alloc = grid.allocatedUe(i);
        if (alloc.has_value()) {
            ++prbsPerUe[alloc.value()];
        }
    }

    double totalBits = 0.0;
    for (const UEContext* ue : ues) {
        const double bits = ue->lastTtiThroughput();   // set by the scheduler

        auto& rec           = m_ueRecords[ue->rnti()];
        rec.rnti            = ue->rnti();
        rec.cumulativeBits += bits;

        const auto it = prbsPerUe.find(ue->rnti());
        if (it != prbsPerUe.end()) {
            rec.prbsAllocated += it->second;
        }
        if (bits > 0.0) {
            ++rec.schedulingCount;
        }

        totalBits += bits;
    }

    m_ttiThroughput.push_back(totalBits);
    ++m_numTtis;
}

// ── Metric computations ──────────────────────────────────────────────────────

double Statistics::totalThroughputBits() const noexcept
{
    double total = 0.0;
    for (const auto& [rnti, rec] : m_ueRecords) {
        total += rec.cumulativeBits;
    }
    return total;
}

double Statistics::avgSystemThroughputPerTti() const noexcept
{
    if (m_numTtis == 0) return 0.0;
    return totalThroughputBits() / static_cast<double>(m_numTtis);
}

double Statistics::jainsFairnessIndex() const noexcept
{
    if (m_ueRecords.empty()) return 1.0;

    double sumX  = 0.0;
    double sumX2 = 0.0;
    for (const auto& [rnti, rec] : m_ueRecords) {
        const double x = rec.cumulativeBits;
        sumX  += x;
        sumX2 += x * x;
    }

    // Guard: all-zero is trivially fair (0/0 → 1.0)
    if (sumX2 < 1e-12) return 1.0;

    const double n = static_cast<double>(m_ueRecords.size());
    return (sumX * sumX) / (n * sumX2);
}

double Statistics::avgThroughputForUe(Rnti rnti) const noexcept
{
    if (m_numTtis == 0) return 0.0;
    const auto it = m_ueRecords.find(rnti);
    if (it == m_ueRecords.end()) return 0.0;
    return it->second.cumulativeBits / static_cast<double>(m_numTtis);
}

// ── printReport ──────────────────────────────────────────────────────────────

void Statistics::printReport() const
{
    // Helper: format RNTI as "0x0001"
    auto fmtRnti = [](Rnti r) -> std::string {
        std::ostringstream oss;
        oss << "0x" << std::hex << std::uppercase
            << std::setfill('0') << std::setw(4) << r;
        return oss.str();
    };

    // Sort UE records by RNTI for deterministic output
    std::vector<const UeRecord*> sorted;
    sorted.reserve(m_ueRecords.size());
    for (const auto& [rnti, rec] : m_ueRecords) {
        sorted.push_back(&rec);
    }
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const UeRecord* a, const UeRecord* b) {
                         return a->rnti < b->rnti;
                     });

    const std::string kSep(56, '-');
    const std::string kBig(56, '=');

    std::cout << "\n" << kBig << "\n";
    std::cout << "  Simulation Report"
              << "  (" << m_numTtis << " TTIs"
              << " / " << m_ueRecords.size() << " UEs)\n";
    std::cout << kBig << "\n\n";

    // Header row
    std::cout << "  " << std::left
              << std::setw(9)  << "RNTI"
              << std::setw(17) << "Avg (bits/TTI)"
              << std::setw(14) << "Total PRBs"
              << std::setw(12) << "Sched TTIs"
              << "\n";
    std::cout << "  " << kSep << "\n";

    for (const UeRecord* rec : sorted) {
        const double avg = (m_numTtis > 0)
                           ? rec->cumulativeBits / static_cast<double>(m_numTtis)
                           : 0.0;
        std::cout << "  " << std::left
                  << std::setw(9)  << fmtRnti(rec->rnti)
                  << std::setw(17) << std::fixed << std::setprecision(1) << avg
                  << std::setw(14) << rec->prbsAllocated
                  << std::setw(12) << rec->schedulingCount
                  << "\n";
    }

    std::cout << "  " << kSep << "\n";
    std::cout << "  System avg throughput : "
              << std::fixed << std::setprecision(1)
              << avgSystemThroughputPerTti() << " bits/TTI\n";
    std::cout << "  Jain's Fairness Index : "
              << std::fixed << std::setprecision(4)
              << jainsFairnessIndex() << "\n";
    std::cout << kBig << "\n\n";
}
