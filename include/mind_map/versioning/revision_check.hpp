#pragma once

#include "mind_map/validation/errors/errors.hpp"
#include "mind_map/utils/id/ids.hpp"

#include <optional>

namespace mind_map::versioning {
    inline StatusCode check_revision(std::optional<Version> expected, Version actual) {
        if (!expected) {
            return StatusCode::Ok;
        }
        return *expected == actual ? StatusCode::Ok : StatusCode::VersionMismatch;
    }
}
