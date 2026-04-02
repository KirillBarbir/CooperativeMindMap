#include "mind_map/validation/rules/access_rules.hpp"

#include "../../include/mind_map/domain/membership.hpp"

namespace mind_map::access {
    bool is_membership_role(Role r) {
        return r == Role::Editor || r == Role::Commenter || r == Role::Viewer;
    }

    bool can_invite(Role effective_role) {
        return effective_role == Role::Owner || effective_role == Role::Editor;
    }

    bool can_change_roles(Role effective_role) { return effective_role == Role::Owner; }

    bool can_edit_structure(Role effective_role) {
        return effective_role == Role::Owner || effective_role == Role::Editor;
    }

    bool can_edit_node_content(Role effective_role) {
        return effective_role == Role::Owner || effective_role == Role::Editor;
    }

    bool can_comment(Role effective_role) {
        return effective_role == Role::Owner || effective_role == Role::Editor ||
               effective_role == Role::Commenter;
    }

    Role effective_role(const UserId &actor, const UserId &owner_id,
                        const std::optional<Role> &member_role) {
        if (actor == owner_id) {
            return Role::Owner;
        }
        if (!member_role) {
            return Role::Viewer;
        }
        return *member_role;
    }
}
