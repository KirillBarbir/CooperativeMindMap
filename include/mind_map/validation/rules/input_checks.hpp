#pragma once

#include "mind_map/validation/errors/errors.hpp"
#include "mind_map/utils/id/ids.hpp"

#include <optional>
#include <string>

namespace mind_map::validation {
    inline StatusCode require_non_empty(const std::string &s) {
        return s.empty() ? StatusCode::InvalidArgument : StatusCode::Ok;
    }

    inline StatusCode require_non_empty(const UserId &a, const SpaceId &s) {
        if (a.empty() || s.empty()) {
            return StatusCode::InvalidArgument;
        }
        return StatusCode::Ok;
    }

    inline StatusCode require_non_empty(const UserId &a, const SpaceId &sp, const std::string &id) {
        if (a.empty() || sp.empty() || id.empty()) {
            return StatusCode::InvalidArgument;
        }
        return StatusCode::Ok;
    }

    inline OperationResult fail(StatusCode c, Version rev = 0) {
        OperationResult r;
        r.code = c;
        r.space_revision = rev;
        return r;
    }

    inline OperationResult ok(Version rev, std::optional<Version> node_ver = {}) {
        OperationResult r;
        r.code = StatusCode::Ok;
        r.space_revision = rev;
        r.node_content_version = std::move(node_ver);
        return r;
    }
}
