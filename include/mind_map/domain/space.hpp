#pragma once

#include "membership.hpp"
#include "mind_map/utils/id/ids.hpp"

#include <vector>

namespace mind_map {
    struct Space {
        SpaceId id;
        UserId owner_id;
        Version revision = 0;
        std::vector<Membership> memberships;
    };
}
