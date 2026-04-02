#pragma once

#include "mind_map/utils/id/ids.hpp"

#include <string>

namespace mind_map {
    struct Node {
        NodeId id;
        std::string content;
        Version content_version = 0;
    };
}
