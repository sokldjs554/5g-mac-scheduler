/**
 * @file SchedulerFactory.hpp
 * @brief Factory function for creating MAC scheduler objects.
 */
#pragma once

#include "scheduler/IScheduler.hpp"

#include <memory>
#include <string_view>

/**
 * @class SchedulerFactory
 * @brief Creates a concrete IScheduler from a configuration string.
 *
 * This is a simple Factory: the caller asks for a scheduler by name and
 * receives a @c std::unique_ptr<IScheduler> without knowing which concrete
 * class was constructed.  Adding a new scheduler algorithm only requires:
 *  1. Implementing a new class that inherits IScheduler.
 *  2. Adding one @c if-branch in SchedulerFactory::create().
 *
 * @par Supported types
 * | String | Scheduler |
 * |--------|-----------|
 * | "rr"   | RoundRobinScheduler |
 * | "pf"   | ProportionalFairScheduler |
 */
class SchedulerFactory {
public:
    /**
     * @brief Create a scheduler by name.
     * @param type Scheduler identifier: @c "rr" or @c "pf".
     * @return     Owning pointer to the requested scheduler.
     * @throws std::invalid_argument if @p type is not recognised.
     */
    [[nodiscard]] static std::unique_ptr<IScheduler> create(std::string_view type);
};
