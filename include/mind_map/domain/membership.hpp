#pragma once

#include "mind_map/utils/id/ids.hpp"

namespace mind_map {
    enum class Role { Owner, Editor, Commenter, Viewer };

    struct Membership {
        UserId user_id;
        Role role = Role::Viewer;
    };
}
