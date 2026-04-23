#include "mind_map/services/mind_map_service.hpp"
#include "mind_map/services/detail/mind_map_context.hpp"

#include "../../include/mind_map/domain/node.hpp"
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
    using validation::fail;
    using validation::ok;
    using validation::require_non_empty;
    using versioning::check_revision;

    OperationResult MindMapService::create_node(UserId actor_id, SpaceId space_id, NodeId &out_node_id,
                                                std::optional<Version> if_match_space_revision) {
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

            const Role eff = access::effective_role(actor_id, sp.owner_id, member_role_lookup(sp, actor_id));
            if (!access::can_edit_structure(eff)) {
                return fail(StatusCode::AccessDenied, sp.revision);
            }

            if (check_revision(if_match_space_revision, sp.revision) != StatusCode::Ok) {
                return fail(StatusCode::VersionMismatch, sp.revision);
            }

            const NodeId nid = make_uuid_string();
            sp.nodes.emplace(nid, NodeRecord{});
            sp.revision += 1;
            out_node_id = nid;
            result = ok(sp.revision);
        }
        return result;
    }

    OperationResult MindMapService::delete_node(UserId actor_id, SpaceId space_id, NodeId node_id,
                                                std::optional<Version> if_match_space_revision) {
        if (require_non_empty(actor_id, space_id, node_id) != StatusCode::Ok) {
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

            if (!sp.nodes.contains(node_id)) {
                return fail(StatusCode::NotFound, sp.revision);
            }

            std::vector<EdgeId> edges_to_erase;
            for (const auto &[eid, ends]: sp.edges) {
                if (ends.first == node_id || ends.second == node_id) {
                    edges_to_erase.push_back(eid);
                }
            }

            for (const auto &eid: edges_to_erase) {
                sp.edges.erase(eid);
            }

            std::vector<CommentId> comments_to_erase;
            for (const auto &[cid, rec]: sp.comments) {
                if (rec.anchor.node_id == node_id) {
                    comments_to_erase.push_back(cid);
                }
            }
            for (const auto &cid: comments_to_erase) {
                sp.comments.erase(cid);
            }

            for (auto &[uid, pr]: sp.presence) {
                (void) uid;
                if (pr.focused_node == node_id) {
                    pr.focused_node.reset();
                    pr.cursor_offset = 0;
                }
            }

            sp.nodes.erase(node_id);
            sp.revision += 1;
            result = ok(sp.revision);
        }
        return result;
    }

    StatusCode MindMapService::get_node(UserId actor_id, SpaceId space_id, NodeId node_id,
                                        Node &out_node) const {
        if (require_non_empty(actor_id, space_id, node_id) != StatusCode::Ok) {
            return StatusCode::InvalidArgument;
        }

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

        auto nit = sp.nodes.find(node_id);
        if (nit == sp.nodes.end()) {
            return StatusCode::NotFound;
        }

        out_node.id = node_id;
        out_node.content = nit->second.content;
        out_node.content_version = nit->second.content_version;
        return StatusCode::Ok;
    }

    OperationResult MindMapService::set_node_content(UserId actor_id, SpaceId space_id, NodeId node_id,
                                                     std::string new_content,
                                                     std::optional<Version> if_match_space_revision,
                                                     std::optional<Version> if_match_node_content_version) {
        if (require_non_empty(actor_id, space_id, node_id) != StatusCode::Ok) {
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
            if (!access::can_edit_node_content(eff)) {
                return fail(StatusCode::AccessDenied, sp.revision);
            }

            if (check_revision(if_match_space_revision, sp.revision) != StatusCode::Ok) {
                return fail(StatusCode::VersionMismatch, sp.revision);
            }

            auto nit = sp.nodes.find(node_id);
            if (nit == sp.nodes.end()) {
                return fail(StatusCode::NotFound, sp.revision);
            }

            if (check_revision(if_match_node_content_version, nit->second.content_version) != StatusCode::Ok) {
                return fail(StatusCode::VersionMismatch, sp.revision);
            }

            nit->second.content = std::move(new_content);
            nit->second.content_version += 1;
            sp.revision += 1;
            result = ok(sp.revision, nit->second.content_version);
        }

        return result;
    }

    OperationResult MindMapService::set_node_title(UserId actor_id, SpaceId space_id, NodeId node_id,
                                                   std::string new_title, std::optional<Version> if_match_space_revision) {
        if (require_non_empty(actor_id, space_id, node_id) != StatusCode::Ok) {
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
            if (!access::can_edit_node_content(eff)) {
                return fail(StatusCode::AccessDenied, sp.revision);
            }

            if (check_revision(if_match_space_revision, sp.revision) != StatusCode::Ok) {
                return fail(StatusCode::VersionMismatch, sp.revision);
            }

            auto nit = sp.nodes.find(node_id);
            if (nit == sp.nodes.end()) {
                return fail(StatusCode::NotFound, sp.revision);
            }

            nit->second.title = std::move(new_title);
            sp.revision += 1;
            result = ok(sp.revision);
        }

        return result;
    }

    OperationResult MindMapService::set_node_position(UserId actor_id, SpaceId space_id, NodeId node_id, double x,
                                                      double y, std::optional<Version> if_match_space_revision) {
        if (require_non_empty(actor_id, space_id, node_id) != StatusCode::Ok) {
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
            if (!access::can_edit_node_content(eff)) {
                return fail(StatusCode::AccessDenied, sp.revision);
            }

            if (check_revision(if_match_space_revision, sp.revision) != StatusCode::Ok) {
                return fail(StatusCode::VersionMismatch, sp.revision);
            }

            auto nit = sp.nodes.find(node_id);
            if (nit == sp.nodes.end()) {
                return fail(StatusCode::NotFound, sp.revision);
            }

            nit->second.x = x;
            nit->second.y = y;
            sp.revision += 1;
            result = ok(sp.revision);
        }

        return result;
    }

    StatusCode MindMapService::list_nodes(UserId actor_id, SpaceId space_id,
                                          std::vector<Node> &out) const {
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

        for (const auto &[nid, rec]: sp.nodes) {
            Node n;
            n.id = nid;
            n.title = rec.title;
            n.content = rec.content;
            n.content_version = rec.content_version;
            n.x = rec.x;
            n.y = rec.y;
            out.push_back(std::move(n));
        }
        std::sort(out.begin(), out.end(), [](const Node &a, const Node &b) { return a.id < b.id; });
        return StatusCode::Ok;
    }
}
