#pragma once

#include "mind_map/utils/id/ids.hpp"

#include <cstddef>
#include <optional>
#include <string>

namespace mind_map {
    struct Presence {
        UserId user_id;
        bool active = false;
        std::optional<NodeId> focused_node;
        std::size_t cursor_offset = 0;
    };
}
