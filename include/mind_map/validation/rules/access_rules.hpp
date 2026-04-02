#pragma once

#include "mind_map/utils/id/ids.hpp"

#include <optional>

#include "../../domain/membership.hpp"

namespace mind_map::access {
    bool is_membership_role(Role r);

    bool can_invite(Role effective_role);

    bool can_change_roles(Role effective_role);

    bool can_edit_structure(Role effective_role);

    bool can_edit_node_content(Role effective_role);

    bool can_comment(Role effective_role);

    Role effective_role(const UserId &actor, const UserId &owner_id, const std::optional<Role> &member_role);
}
