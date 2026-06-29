#include "scheduler/SchedulerFactory.hpp"
#include "scheduler/ProportionalFairScheduler.hpp"
#include "scheduler/RoundRobinScheduler.hpp"

#include <stdexcept>
#include <string>

std::unique_ptr<IScheduler> SchedulerFactory::create(std::string_view type)
{
    if (type == "rr") return std::make_unique<RoundRobinScheduler>();
    if (type == "pf") return std::make_unique<ProportionalFairScheduler>();

    throw std::invalid_argument(
        "SchedulerFactory: unknown scheduler type '"
        + std::string(type)
        + "'. Supported: \"rr\", \"pf\".");
}
