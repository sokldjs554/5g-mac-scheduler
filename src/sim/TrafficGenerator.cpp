#include "sim/TrafficGenerator.hpp"

#include <algorithm>  // std::max

TrafficGenerator::TrafficGenerator(double arrivalRateBitsPerTti, uint32_t seed)
    : m_arrivalRate{ arrivalRateBitsPerTti }
    , m_rng{ seed }
    , m_dist{ std::max(arrivalRateBitsPerTti, 1.0) }   // Poisson mean must be > 0
{}

void TrafficGenerator::generate(std::vector<UEContext*>& ues)
{
    for (UEContext* ue : ues) {
        const uint32_t bits  = m_dist(m_rng);
        const uint32_t bytes = (bits + 7u) / 8u;   // ceil(bits / 8)
        ue->setBsr(ue->bsr() + bytes);
    }
}
