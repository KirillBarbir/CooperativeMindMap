#include "mind_map/services/mind_map_service.hpp"
#include "mind_map/services/detail/mind_map_context.hpp"

#include "../../include/mind_map/domain/edge.hpp"
#include "mind_map/services/access/access_helpers.hpp"
#include "mind_map/utils/id/uuid.hpp"
#include "mind_map/validation/rules/access_rules.hpp"
#include "mind_map/validation/rules/input_checks.hpp"
#include "../../include/mind_map/versioning/revision_check.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace mind_map {
    using access::has_access;
    using access::member_role_lookup;
    using access::normalize_edge_endpoints;
    using validation::fail;
    using validation::ok;
    using validation::require_non_empty;
    using versioning::check_revision;

    OperationResult MindMapService::create_edge(UserId actor_id, SpaceId space_id, NodeId from, NodeId to,
                                                EdgeId &out_edge_id, std::optional<Version> if_match_space_revision) {
        if (require_non_empty(actor_id, space_id) != StatusCode::Ok || from.empty() || to.empty()) {
            return fail(StatusCode::InvalidArgument);
        }

        if (from == to) {
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

            const Role eff = access::effective_role(actor_id, sp.owner_id, member_role_lookup(sp, actor_id));
            if (!access::can_edit_structure(eff)) {
                return fail(StatusCode::AccessDenied, sp.revision);
            }

            if (check_revision(if_match_space_revision, sp.revision) != StatusCode::Ok) {
                return fail(StatusCode::VersionMismatch, sp.revision);
            }

            if (!sp.nodes.contains(from) || !sp.nodes.contains(to)) {
                return fail(StatusCode::NotFound, sp.revision);
            }

            NodeId lo;
            NodeId hi;
            normalize_edge_endpoints(from, to, lo, hi);
            for (const auto &[eid, ends]: sp.edges) {
                if (ends.first == lo && ends.second == hi) {
                    return fail(StatusCode::Conflict, sp.revision);
                }
            }

            const EdgeId eid = make_uuid_string();
            sp.edges.emplace(eid, std::pair<NodeId, NodeId>{lo, hi});
            sp.revision += 1;
            out_edge_id = eid;
            result = ok(sp.revision);
        }
        return result;
    }

    OperationResult MindMapService::delete_edge(UserId actor_id, SpaceId space_id, EdgeId edge_id,
                                                std::optional<Version> if_match_space_revision) {
        if (require_non_empty(actor_id, space_id) != StatusCode::Ok || edge_id.empty()) {
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

            const Role eff = access::effective_role(actor_id, sp.owner_id, member_role_lookup(sp, actor_id));
            if (!access::can_edit_structure(eff)) {
                return fail(StatusCode::AccessDenied, sp.revision);
            }

            if (check_revision(if_match_space_revision, sp.revision) != StatusCode::Ok) {
                return fail(StatusCode::VersionMismatch, sp.revision);
            }

            if (!sp.edges.contains(edge_id)) {
                return fail(StatusCode::NotFound, sp.revision);
            }

            sp.edges.erase(edge_id);
            sp.revision += 1;
            result = ok(sp.revision);
        }

        return result;
    }

    StatusCode MindMapService::list_edges(UserId actor_id, SpaceId space_id, std::vector<Edge> &out) const {
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

        for (const auto &[eid, ends]: sp.edges) {
            Edge e;
            e.id = eid;
            e.from = ends.first;
            e.to = ends.second;
            out.push_back(std::move(e));
        }

        std::sort(out.begin(), out.end(), [](const Edge &a, const Edge &b) { return a.id < b.id; });
        return StatusCode::Ok;
    }
}
