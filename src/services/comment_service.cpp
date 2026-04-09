#include "mind_map/services/mind_map_service.hpp"
#include "mind_map/services/detail/mind_map_context.hpp"

#include "../../include/mind_map/domain/comment.hpp"
#include "mind_map/services/access/access_helpers.hpp"
#include "mind_map/utils/id/uuid.hpp"
#include "mind_map/utils/time/clock.hpp"
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

    OperationResult MindMapService::add_comment(UserId actor_id, SpaceId space_id, CommentAnchor anchor,
                                                std::string text, CommentId &out_comment_id,
                                                std::optional<Version> if_match_space_revision) {
        if (require_non_empty(actor_id, space_id) != StatusCode::Ok || anchor.node_id.empty() || text.empty()) {
            return fail(StatusCode::InvalidArgument);
        }

        if (anchor.kind == CommentTargetKind::TextRange) {
            if (anchor.range_end < anchor.range_start) {
                return fail(StatusCode::InvalidArgument);
            }
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
            if (!access::can_comment(eff)) {
                return fail(StatusCode::AccessDenied, sp.revision);
            }

            if (check_revision(if_match_space_revision, sp.revision) != StatusCode::Ok) {
                return fail(StatusCode::VersionMismatch, sp.revision);
            }

            auto nit = sp.nodes.find(anchor.node_id);
            if (nit == sp.nodes.end()) {
                return fail(StatusCode::NotFound, sp.revision);
            }

            if (anchor.kind == CommentTargetKind::TextRange) {
                const std::size_t len = nit->second.content.size();
                if (anchor.range_start > len || anchor.range_end > len) {
                    return fail(StatusCode::InvalidArgument, sp.revision);
                }
            }

            const CommentId cid = make_uuid_string();
            CommentRecord rec;
            rec.id = cid;
            rec.author_id = actor_id;
            rec.anchor = anchor;
            rec.text = std::move(text);
            rec.created_at = util_time::now();
            sp.comments.emplace(cid, std::move(rec));
            sp.revision += 1;
            out_comment_id = cid;
            result = ok(sp.revision);
        }
        return result;
    }

    StatusCode MindMapService::list_comments(UserId actor_id, SpaceId space_id, NodeId node_id,
                                             std::vector<Comment> &out) const {
        if (require_non_empty(actor_id, space_id, node_id) != StatusCode::Ok) {
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

        std::vector<Comment> tmp;
        for (const auto &[cid, rec]: sp.comments) {
            if (rec.anchor.node_id != node_id) {
                continue;
            }

            Comment c;
            c.id = rec.id;
            c.author_id = rec.author_id;
            c.anchor = rec.anchor;
            c.text = rec.text;
            c.created_at = rec.created_at;
            tmp.push_back(std::move(c));
        }

        std::sort(tmp.begin(), tmp.end(),
                  [](const Comment &a, const Comment &b) { return a.created_at < b.created_at; });
        out = std::move(tmp);
        return StatusCode::Ok;
    }
}
