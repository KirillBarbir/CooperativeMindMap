#pragma once

#include "mind_map/utils/id/ids.hpp"

namespace mind_map {
    struct Edge {
        EdgeId id;
        NodeId from;
        NodeId to;
    };
}
