#include "core/HARQManager.hpp"

std::optional<uint8_t> HARQManager::findIdleProcess() const noexcept {
    for (uint8_t i = 0; i < kNumProcesses; ++i) {
        if (m_processes[i].state == HarqProcessState::Idle) {
            return i;
        }
    }
    return std::nullopt;
}

bool HARQManager::startTransmission(uint8_t pid) noexcept {
    if (pid >= kNumProcesses) return false;
    if (m_processes[pid].state != HarqProcessState::Idle) return false;

    m_processes[pid].state     = HarqProcessState::WaitingAck;
    m_processes[pid].retxCount = 0;
    return true;
}

void HARQManager::receiveAck(uint8_t pid) noexcept {
    if (pid >= kNumProcesses) return;
    m_processes[pid].state     = HarqProcessState::Idle;
    m_processes[pid].retxCount = 0;
}

void HARQManager::receiveNack(uint8_t pid) noexcept {
    if (pid >= kNumProcesses) return;
    if (m_processes[pid].state != HarqProcessState::WaitingAck) return;
    m_processes[pid].state = HarqProcessState::Nacked;
}

bool HARQManager::retransmit(uint8_t pid) noexcept {
    if (pid >= kNumProcesses) return false;
    if (m_processes[pid].state != HarqProcessState::Nacked) return false;

    ++m_processes[pid].retxCount;

    if (m_processes[pid].retxCount > kMaxRetransmissions) {
        // Packet flushed: reset the process so it is available again.
        m_processes[pid] = HarqProcess{};
        return false;
    }

    m_processes[pid].state = HarqProcessState::WaitingAck;
    return true;
}

HarqProcessState HARQManager::state(uint8_t pid) const noexcept {
    if (pid >= kNumProcesses) return HarqProcessState::Idle;
    return m_processes[pid].state;
}

uint8_t HARQManager::retxCount(uint8_t pid) const noexcept {
    if (pid >= kNumProcesses) return 0;
    return m_processes[pid].retxCount;
}

bool HARQManager::hasNackedProcess() const noexcept {
    for (const auto& p : m_processes) {
        if (p.state == HarqProcessState::Nacked) return true;
    }
    return false;
}
