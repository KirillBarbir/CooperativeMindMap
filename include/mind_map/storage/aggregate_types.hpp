#pragma once

#include "../domain/comment.hpp"
#include "../domain/presence.hpp"
#include "mind_map/utils/id/ids.hpp"

#include <chrono>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace mind_map {
    struct NodeRecord {
        std::string title = "New Node";
        std::string content;
        Version content_version = 1;
        double x = 0;
        double y = 0;
    };

    struct CommentRecord {
        CommentId id;
        UserId author_id;
        CommentAnchor anchor;
        std::string text;
        std::chrono::system_clock::time_point created_at;
    };

    struct SpaceAggregate {
        mutable std::shared_mutex mutex;
        UserId owner_id;
        Version revision = 1;

        std::unordered_map<UserId, Role> members;
        std::unordered_map<NodeId, NodeRecord> nodes;
        std::unordered_map<EdgeId, std::pair<NodeId, NodeId> > edges;
        std::unordered_map<CommentId, CommentRecord> comments;
        std::unordered_map<UserId, Presence> presence;
    };
}
