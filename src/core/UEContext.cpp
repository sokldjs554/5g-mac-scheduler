#include "core/UEContext.hpp"

#include <sstream>
#include <stdexcept>

UEContext::UEContext(Rnti rnti)
    : m_rnti{rnti}
{}

void UEContext::setCqi(uint8_t cqi) {
    if (cqi < kMinCqi || cqi > kMaxCqi) {
        std::ostringstream oss;
        oss << "CQI " << static_cast<int>(cqi)
            << " out of range [" << static_cast<int>(kMinCqi)
            << ", " << static_cast<int>(kMaxCqi) << "]";
        throw std::out_of_range(oss.str());
    }
    m_cqi = cqi;
}

void UEContext::setBsr(uint32_t bytes) noexcept {
    m_bsr = bytes;
}

void UEContext::updateAvgThroughput(double instantBits) noexcept {
    m_lastTtiThroughput = instantBits;   // snapshot for Statistics
    m_avgThroughput = (1.0 - kEmaAlpha) * m_avgThroughput
                    + kEmaAlpha * instantBits;
}

void UEContext::recordScheduled(TtiNumber tti) noexcept {
    ++m_schedulingCount;
    m_lastScheduledTti = tti;
}
