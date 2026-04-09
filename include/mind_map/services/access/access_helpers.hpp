#pragma once

#include "../../storage/aggregate_types.hpp"
#include "mind_map/utils/id/ids.hpp"

#include <optional>

namespace mind_map::access {
    inline bool has_access(const UserId &actor, const SpaceAggregate &space) {
        if (actor == space.owner_id) {
            return true;
        }
        return space.members.find(actor) != space.members.end();
    }

    inline std::optional<Role> member_role_lookup(const SpaceAggregate &space, const UserId &actor) {
        auto it = space.members.find(actor);
        if (it == space.members.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    inline void normalize_edge_endpoints(const NodeId &x, const NodeId &y, NodeId &out_lo,
                                         NodeId &out_hi) {
        if (x <= y) {
            out_lo = x;
            out_hi = y;
        } else {
            out_lo = y;
            out_hi = x;
        }
    }
}
