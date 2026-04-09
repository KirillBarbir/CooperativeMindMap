#include "mind_map/services/mind_map_service.hpp"
#include "mind_map/services/detail/mind_map_context.hpp"

#include "../../include/mind_map/domain/membership.hpp"
#include "../../include/mind_map/domain/space.hpp"
#include "mind_map/services/access/access_helpers.hpp"
#include "mind_map/utils/id/uuid.hpp"
#include "mind_map/validation/rules/access_rules.hpp"
#include "mind_map/validation/rules/input_checks.hpp"
#include "../../include/mind_map/versioning/revision_check.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace mind_map {
    using access::effective_role;
    using access::has_access;
    using access::member_role_lookup;
    using validation::fail;
    using validation::ok;
    using validation::require_non_empty;
    using versioning::check_revision;

    OperationResult MindMapService::create_space(UserId owner_user_id, SpaceId &out_space_id) {
        if (require_non_empty(owner_user_id) != StatusCode::Ok) {
            return fail(StatusCode::InvalidArgument);
        }

        const SpaceId id = make_uuid_string();
        auto agg = std::make_unique<SpaceAggregate>();
        agg->owner_id = owner_user_id;
        agg->revision = 1;

        ctx_->spaces.emplace(id, std::move(agg));
        out_space_id = id;
        return ok(1);
    }

    OperationResult MindMapService::invite_user(UserId actor_id, SpaceId space_id, UserId invitee_id, Role role,
                                                std::optional<Version> if_match_space_revision) {
        if (require_non_empty(actor_id, space_id, invitee_id) != StatusCode::Ok) {
            return fail(StatusCode::InvalidArgument);
        }

        if (!access::is_membership_role(role)) {
            return fail(StatusCode::InvalidArgument);
        }

        OperationResult out;
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

            const Role eff = effective_role(actor_id, sp.owner_id, member_role_lookup(sp, actor_id));
            if (!access::can_invite(eff)) {
                return fail(StatusCode::AccessDenied, sp.revision);
            }

            if (check_revision(if_match_space_revision, sp.revision) != StatusCode::Ok) {
                return fail(StatusCode::VersionMismatch, sp.revision);
            }

            if (invitee_id == sp.owner_id) {
                return fail(StatusCode::Conflict, sp.revision);
            }

            if (sp.members.contains(invitee_id)) {
                return fail(StatusCode::Conflict, sp.revision);
            }

            sp.members.emplace(invitee_id, role);
            sp.revision += 1;
            out = ok(sp.revision);
        }
        return out;
    }

    OperationResult MindMapService::set_member_role(UserId actor_id, SpaceId space_id, UserId member_id, Role new_role,
                                                    std::optional<Version> if_match_space_revision) {
        if (require_non_empty(actor_id, space_id, member_id) != StatusCode::Ok) {
            return fail(StatusCode::InvalidArgument);
        }

        if (!access::is_membership_role(new_role)) {
            return fail(StatusCode::InvalidArgument);
        }

        OperationResult out;
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

            const Role eff = effective_role(actor_id, sp.owner_id, member_role_lookup(sp, actor_id));
            if (!access::can_change_roles(eff)) {
                return fail(StatusCode::AccessDenied, sp.revision);
            }

            if (check_revision(if_match_space_revision, sp.revision) != StatusCode::Ok) {
                return fail(StatusCode::VersionMismatch, sp.revision);
            }

            if (member_id == sp.owner_id) {
                return fail(StatusCode::InvalidArgument, sp.revision);
            }

            auto mit = sp.members.find(member_id);
            if (mit == sp.members.end()) {
                return fail(StatusCode::NotFound, sp.revision);
            }

            mit->second = new_role;
            sp.revision += 1;
            out = ok(sp.revision);
        }
        return out;
    }

    StatusCode MindMapService::get_space(UserId actor_id, SpaceId space_id, Space &out) const {
        if (require_non_empty(actor_id, space_id) != StatusCode::Ok) {
            return StatusCode::InvalidArgument;
        }

        auto it = ctx_->spaces.find(space_id);
        if (it == ctx_->spaces.end()) {
            return StatusCode::NotFound;
        }

        const SpaceAggregate &sp = *it->second;
        std::shared_lock<std::shared_mutex> lock(sp.mutex);
        if (!has_access(actor_id, sp)) {
            return StatusCode::AccessDenied;
        }

        out.id = space_id;
        out.owner_id = sp.owner_id;
        out.revision = sp.revision;
        out.memberships.clear();
        for (const auto &[uid, role]: sp.members) {
            out.memberships.push_back(Membership{uid, role});
        }

        std::sort(out.memberships.begin(), out.memberships.end(),
                  [](const Membership &a, const Membership &b) { return a.user_id < b.user_id; });
        return StatusCode::Ok;
    }
}
