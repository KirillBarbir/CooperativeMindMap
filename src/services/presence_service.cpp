#include "mind_map/services/mind_map_service.hpp"
#include "mind_map/services/detail/mind_map_context.hpp"

#include "mind_map/domain/presence.hpp"
#include "mind_map/services/access/access_helpers.hpp"
#include "mind_map/validation/rules/input_checks.hpp"

#include <algorithm>
#include <vector>

namespace mind_map {
    using access::has_access;
    using validation::fail;
    using validation::ok;
    using validation::require_non_empty;

    OperationResult MindMapService::update_presence(UserId actor_id, SpaceId space_id,
                                                    std::optional<NodeId> focused_node,
                                                    std::size_t cursor_offset) {
        if (require_non_empty(actor_id, space_id) != StatusCode::Ok) {
            return fail(StatusCode::InvalidArgument);
        }

        OperationResult result;
        std::shared_lock<std::shared_mutex> map_lock(ctx_->spaces_mutex);
        auto it = ctx_->spaces.find(space_id);
        if (it == ctx_->spaces.end()) {
            return fail(StatusCode::NotFound);
        }
        SpaceAggregate &sp = *it->second;

        {
            std::unique_lock<std::shared_mutex> lock(sp.mutex);
            if (!has_access(actor_id, sp)) {
                return fail(StatusCode::AccessDenied, sp.revision);
            }

            if (focused_node) {
                if (!sp.nodes.contains(*focused_node)) {
                    return fail(StatusCode::NotFound, sp.revision);
                }

                const std::size_t len = sp.nodes.at(*focused_node).content.size();
                if (cursor_offset > len) {
                    return fail(StatusCode::InvalidArgument, sp.revision);
                }
            } else if (cursor_offset != 0) {
                return fail(StatusCode::InvalidArgument, sp.revision);
            }

            Presence &pr = sp.presence[actor_id];
            pr.user_id = actor_id;
            pr.active = true;
            pr.focused_node = focused_node;
            pr.cursor_offset = cursor_offset;
            result = ok(sp.revision);
        }
        return result;
    }

    OperationResult MindMapService::clear_presence(UserId actor_id, SpaceId space_id) {
        if (require_non_empty(actor_id, space_id) != StatusCode::Ok) {
            return fail(StatusCode::InvalidArgument);
        }

        OperationResult result;
        std::shared_lock<std::shared_mutex> map_lock(ctx_->spaces_mutex);
        auto sit = ctx_->spaces.find(space_id);
        if (sit == ctx_->spaces.end()) {
            return fail(StatusCode::NotFound);
        }
        SpaceAggregate &sp = *sit->second;

        {
            std::unique_lock<std::shared_mutex> lock(sp.mutex);
            if (!has_access(actor_id, sp)) {
                return fail(StatusCode::AccessDenied, sp.revision);
            }

            sp.presence.erase(actor_id);
            result = ok(sp.revision);
        }
        return result;
    }

    StatusCode MindMapService::list_presence(UserId actor_id, SpaceId space_id,
                                             std::vector<Presence> &out) const {
        if (require_non_empty(actor_id, space_id) != StatusCode::Ok) {
            return StatusCode::InvalidArgument;
        }

        out.clear();
        std::shared_lock<std::shared_mutex> map_lock(ctx_->spaces_mutex);
        auto it = ctx_->spaces.find(space_id);
        if (it == ctx_->spaces.end()) {
            return StatusCode::NotFound;
        }

        const SpaceAggregate &sp = *it->second;
        std::shared_lock<std::shared_mutex> lock(sp.mutex);
        if (!has_access(actor_id, sp)) {
            return StatusCode::AccessDenied;
        }

        for (const auto &[uid, pr]: sp.presence) {
            if (pr.active) {
                out.push_back(pr);
            }
        }

        std::sort(out.begin(), out.end(),
                  [](const Presence &a, const Presence &b) { return a.user_id < b.user_id; });
        return StatusCode::Ok;
    }
}
