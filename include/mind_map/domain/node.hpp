#pragma once

#include "mind_map/utils/id/ids.hpp"

#include <string>

namespace mind_map {
    struct Node {
        NodeId id;
        std::string title = "New Node";
        std::string content;
        Version content_version = 0;
        double x = 0;
        double y = 0;
    };
}
