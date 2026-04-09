#pragma once

#include "../../storage/aggregate_types.hpp"
#include "mind_map/utils/id/ids.hpp"

#include <memory>
#include <unordered_map>

namespace mind_map {
    struct MindMapContext {
        std::unordered_map<SpaceId, std::unique_ptr<SpaceAggregate>> spaces;
    };
}
