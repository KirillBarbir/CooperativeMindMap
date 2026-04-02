#pragma once

#include "mind_map/utils/id/ids.hpp"

#include <optional>

namespace mind_map {
    enum class StatusCode {
        Ok,
        NotFound,
        AccessDenied,
        InvalidArgument,
        Conflict,
        VersionMismatch,
    };

    struct OperationResult {
        StatusCode code = StatusCode::Ok;
        Version space_revision = 0;
        std::optional<Version> node_content_version;
    };
}
