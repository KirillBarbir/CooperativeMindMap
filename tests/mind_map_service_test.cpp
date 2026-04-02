#include "mind_map/services/mind_map_service.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace mind_map;

TEST(MindMapService, OutsiderDenied) {
    MindMapService svc;
    SpaceId sid;
    ASSERT_EQ(svc.create_space("owner", sid).code, StatusCode::Ok);
    Space sp;
    EXPECT_EQ(svc.get_space("stranger", sid, sp), StatusCode::AccessDenied);
}

TEST(MindMapService, InviteWithOwnerRoleRejected) {
    MindMapService svc;
    SpaceId sid;
    ASSERT_EQ(svc.create_space("owner", sid).code, StatusCode::Ok);
    OperationResult r = svc.invite_user("owner", sid, "x", Role::Owner, 1);
    EXPECT_EQ(r.code, StatusCode::InvalidArgument);
}

TEST(MindMapService, CreateSpaceGetSpaceAndRevision) {
    MindMapService svc;
    SpaceId sid;
    OperationResult r = svc.create_space("owner", sid);
    ASSERT_EQ(r.code, StatusCode::Ok);
    EXPECT_EQ(r.space_revision, 1u);

    Space sp;
    ASSERT_EQ(svc.get_space("owner", sid, sp), StatusCode::Ok);
    EXPECT_EQ(sp.owner_id, "owner");
    EXPECT_EQ(sp.revision, 1u);
    EXPECT_TRUE(sp.memberships.empty());
}

TEST(MindMapService, InviteEditorAndViewerRoles) {
    MindMapService svc;
    SpaceId sid;
    ASSERT_EQ(svc.create_space("owner", sid).code, StatusCode::Ok);

    OperationResult a = svc.invite_user("owner", sid, "ed", Role::Editor, 1);
    ASSERT_EQ(a.code, StatusCode::Ok);
    EXPECT_EQ(a.space_revision, 2u);

    OperationResult b = svc.invite_user("ed", sid, "vi", Role::Viewer, 2);
    ASSERT_EQ(b.code, StatusCode::Ok);
    EXPECT_EQ(b.space_revision, 3u);

    Space sp;
    ASSERT_EQ(svc.get_space("vi", sid, sp), StatusCode::Ok);
    ASSERT_EQ(sp.memberships.size(), 2u);
}

TEST(MindMapService, ViewerCannotInviteOrEditStructure) {
    MindMapService svc;
    SpaceId sid;
    ASSERT_EQ(svc.create_space("owner", sid).code, StatusCode::Ok);
    ASSERT_EQ(svc.invite_user("owner", sid, "vi", Role::Viewer, 1).code, StatusCode::Ok);

    OperationResult inv = svc.invite_user("vi", sid, "x", Role::Editor, 2);
    EXPECT_EQ(inv.code, StatusCode::AccessDenied);

    NodeId nid;
    EXPECT_EQ(svc.create_node("vi", sid, nid, 2).code, StatusCode::AccessDenied);
}

TEST(MindMapService, OnlyOwnerChangesRoles) {
    MindMapService svc;
    SpaceId sid;
    ASSERT_EQ(svc.create_space("owner", sid).code, StatusCode::Ok);
    ASSERT_EQ(svc.invite_user("owner", sid, "ed", Role::Editor, 1).code, StatusCode::Ok);
    ASSERT_EQ(svc.invite_user("owner", sid, "vi", Role::Viewer, 2).code, StatusCode::Ok);

    OperationResult denied = svc.set_member_role("ed", sid, "vi", Role::Commenter, 3);
    EXPECT_EQ(denied.code, StatusCode::AccessDenied);

    OperationResult ok = svc.set_member_role("owner", sid, "vi", Role::Commenter, 3);
    ASSERT_EQ(ok.code, StatusCode::Ok);
    EXPECT_EQ(ok.space_revision, 4u);
}

TEST(MindMapService, DuplicateEdgeConflict) {
    MindMapService svc;
    SpaceId sid;
    ASSERT_EQ(svc.create_space("owner", sid).code, StatusCode::Ok);
    NodeId a;
    NodeId b;
    ASSERT_EQ(svc.create_node("owner", sid, a, 1).code, StatusCode::Ok);
    ASSERT_EQ(svc.create_node("owner", sid, b, 2).code, StatusCode::Ok);

    EdgeId e1;
    ASSERT_EQ(svc.create_edge("owner", sid, a, b, e1, 3).code, StatusCode::Ok);
    EdgeId e2;
    EXPECT_EQ(svc.create_edge("owner", sid, b, a, e2, 4).code, StatusCode::Conflict);
}

TEST(MindMapService, NodeCrudAndContentVersion) {
    MindMapService svc;
    SpaceId sid;
    ASSERT_EQ(svc.create_space("owner", sid).code, StatusCode::Ok);

    NodeId n;
    ASSERT_EQ(svc.create_node("owner", sid, n, 1).code, StatusCode::Ok);

    Node node;
    ASSERT_EQ(svc.get_node("owner", sid, n, node), StatusCode::Ok);
    EXPECT_EQ(node.content_version, 1u);

    OperationResult w = svc.set_node_content("owner", sid, n, "hello", std::nullopt, std::nullopt);
    ASSERT_EQ(w.code, StatusCode::Ok);
    ASSERT_TRUE(w.node_content_version.has_value());
    EXPECT_EQ(*w.node_content_version, 2u);

    ASSERT_EQ(svc.get_node("owner", sid, n, node), StatusCode::Ok);
    EXPECT_EQ(node.content, "hello");
    EXPECT_EQ(node.content_version, 2u);

    ASSERT_EQ(svc.delete_node("owner", sid, n, w.space_revision).code, StatusCode::Ok);
    EXPECT_EQ(svc.get_node("owner", sid, n, node), StatusCode::NotFound);
}

TEST(MindMapService, CommenterCommentsButNoContentEdit) {
    MindMapService svc;
    SpaceId sid;
    ASSERT_EQ(svc.create_space("owner", sid).code, StatusCode::Ok);
    ASSERT_EQ(svc.invite_user("owner", sid, "com", Role::Commenter, 1).code, StatusCode::Ok);

    NodeId n;
    ASSERT_EQ(svc.create_node("owner", sid, n, 2).code, StatusCode::Ok);

    EXPECT_EQ(svc.set_node_content("com", sid, n, "x", std::nullopt, std::nullopt).code,
              StatusCode::AccessDenied);

    CommentAnchor a;
    a.kind = CommentTargetKind::Node;
    a.node_id = n;
    CommentId cid;
    ASSERT_EQ(svc.add_comment("com", sid, a, "hi", cid, 3).code, StatusCode::Ok);

    std::vector<Comment> list;
    ASSERT_EQ(svc.list_comments("owner", sid, n, list), StatusCode::Ok);
    ASSERT_EQ(list.size(), 1u);
}

TEST(MindMapService, ViewerCannotComment) {
    MindMapService svc;
    SpaceId sid;
    ASSERT_EQ(svc.create_space("owner", sid).code, StatusCode::Ok);
    ASSERT_EQ(svc.invite_user("owner", sid, "vi", Role::Viewer, 1).code, StatusCode::Ok);
    NodeId n;
    ASSERT_EQ(svc.create_node("owner", sid, n, 2).code, StatusCode::Ok);

    CommentAnchor a;
    a.kind = CommentTargetKind::Node;
    a.node_id = n;
    CommentId cid;
    EXPECT_EQ(svc.add_comment("vi", sid, a, "nope", cid, 3).code, StatusCode::AccessDenied);
}

TEST(MindMapService, SpaceRevisionMismatch) {
    MindMapService svc;
    SpaceId sid;
    ASSERT_EQ(svc.create_space("owner", sid).code, StatusCode::Ok);

    OperationResult bad = svc.invite_user("owner", sid, "u1", Role::Editor, 99);
    EXPECT_EQ(bad.code, StatusCode::VersionMismatch);
    EXPECT_EQ(bad.space_revision, 1u);

    OperationResult ok = svc.invite_user("owner", sid, "u1", Role::Editor, 1);
    ASSERT_EQ(ok.code, StatusCode::Ok);
}

TEST(MindMapService, NodeContentVersionMismatch) {
    MindMapService svc;
    SpaceId sid;
    ASSERT_EQ(svc.create_space("owner", sid).code, StatusCode::Ok);
    NodeId n;
    ASSERT_EQ(svc.create_node("owner", sid, n, 1).code, StatusCode::Ok);

    OperationResult bad = svc.set_node_content("owner", sid, n, "a", std::nullopt, Version{99});
    EXPECT_EQ(bad.code, StatusCode::VersionMismatch);

    OperationResult ok = svc.set_node_content("owner", sid, n, "a", std::nullopt, Version{1});
    ASSERT_EQ(ok.code, StatusCode::Ok);
}

TEST(MindMapService, PresenceTracking) {
    MindMapService svc;
    SpaceId sid;
    ASSERT_EQ(svc.create_space("owner", sid).code, StatusCode::Ok);
    ASSERT_EQ(svc.invite_user("owner", sid, "peer", Role::Editor, 1).code, StatusCode::Ok);
    NodeId n;
    ASSERT_EQ(svc.create_node("owner", sid, n, 2).code, StatusCode::Ok);
    ASSERT_EQ(svc.set_node_content("owner", sid, n, "abcd", std::nullopt, std::nullopt).code,
              StatusCode::Ok);

    ASSERT_EQ(svc.update_presence("peer", sid, n, 2).code, StatusCode::Ok);

    std::vector<Presence> pr;
    ASSERT_EQ(svc.list_presence("owner", sid, pr), StatusCode::Ok);
    ASSERT_EQ(pr.size(), 1u);
    EXPECT_EQ(pr[0].user_id, "peer");
    ASSERT_TRUE(pr[0].focused_node.has_value());
    EXPECT_EQ(*pr[0].focused_node, n);
}
