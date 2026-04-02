#pragma once

#include "mind_map/utils/id/ids.hpp"

#include <chrono>
#include <string>

namespace mind_map {
    enum class CommentTargetKind { Node, TextRange };

    struct CommentAnchor {
        CommentTargetKind kind = CommentTargetKind::Node;
        NodeId node_id;
        std::size_t range_start = 0;
        std::size_t range_end = 0;
    };

    struct Comment {
        CommentId id;
        UserId author_id;
        CommentAnchor anchor;
        std::string text;
        std::chrono::system_clock::time_point created_at;
    };
}
