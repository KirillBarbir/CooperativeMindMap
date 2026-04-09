#pragma once

#include "../domain/comment.hpp"
#include "../domain/edge.hpp"
#include "../domain/node.hpp"
#include "../domain/presence.hpp"
#include "../domain/space.hpp"
#include "mind_map/validation/errors/errors.hpp"
#include "mind_map/utils/id/ids.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mind_map {
    struct MindMapContext;

    class MindMapService {
    public:
        MindMapService();

        ~MindMapService();

        MindMapService(const MindMapService &) = delete;

        MindMapService &operator=(const MindMapService &) = delete;

        OperationResult create_space(UserId owner_user_id, SpaceId &out_space_id);

        OperationResult invite_user(UserId actor_id, SpaceId space_id, UserId invitee_id, Role role,
                                    std::optional<Version> if_match_space_revision = {});

        OperationResult set_member_role(UserId actor_id, SpaceId space_id, UserId member_id, Role new_role,
                                        std::optional<Version> if_match_space_revision = {});

        OperationResult create_node(UserId actor_id, SpaceId space_id, NodeId &out_node_id,
                                    std::optional<Version> if_match_space_revision = {});

        OperationResult delete_node(UserId actor_id, SpaceId space_id, NodeId node_id,
                                    std::optional<Version> if_match_space_revision = {});

        StatusCode get_node(UserId actor_id, SpaceId space_id, NodeId node_id, Node &out_node) const;

        OperationResult set_node_content(UserId actor_id, SpaceId space_id, NodeId node_id, std::string new_content,
                                         std::optional<Version> if_match_space_revision = {},
                                         std::optional<Version> if_match_node_content_version = {});

        OperationResult create_edge(UserId actor_id, SpaceId space_id, NodeId from, NodeId to, EdgeId &out_edge_id,
                                    std::optional<Version> if_match_space_revision = {});

        OperationResult delete_edge(UserId actor_id, SpaceId space_id, EdgeId edge_id,
                                    std::optional<Version> if_match_space_revision = {});

        OperationResult add_comment(UserId actor_id, SpaceId space_id, CommentAnchor anchor, std::string text,
                                    CommentId &out_comment_id, std::optional<Version> if_match_space_revision = {});

        StatusCode list_comments(UserId actor_id, SpaceId space_id, NodeId node_id, std::vector<Comment> &out) const;

        OperationResult update_presence(UserId actor_id, SpaceId space_id, std::optional<NodeId> focused_node,
                                        std::size_t cursor_offset);

        OperationResult clear_presence(UserId actor_id, SpaceId space_id);

        StatusCode list_presence(UserId actor_id, SpaceId space_id, std::vector<Presence> &out) const;

        StatusCode get_space(UserId actor_id, SpaceId space_id, Space &out) const;

        StatusCode list_nodes(UserId actor_id, SpaceId space_id, std::vector<Node> &out) const;

        StatusCode list_edges(UserId actor_id, SpaceId space_id, std::vector<Edge> &out) const;

    private:
        std::unique_ptr<MindMapContext> ctx_;
    };
}
