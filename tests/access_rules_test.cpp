#include "mind_map/validation/rules/access_rules.hpp"

#include <gtest/gtest.h>


using namespace mind_map;
using namespace mind_map::access;

TEST(AccessRules, OwnerCapabilities) {
    EXPECT_TRUE(can_invite(Role::Owner));
    EXPECT_TRUE(can_change_roles(Role::Owner));
    EXPECT_TRUE(can_edit_structure(Role::Owner));
    EXPECT_TRUE(can_edit_node_content(Role::Owner));
    EXPECT_TRUE(can_comment(Role::Owner));
}

TEST(AccessRules, EditorCapabilities) {
    EXPECT_TRUE(can_invite(Role::Editor));
    EXPECT_FALSE(can_change_roles(Role::Editor));
    EXPECT_TRUE(can_edit_structure(Role::Editor));
    EXPECT_TRUE(can_edit_node_content(Role::Editor));
    EXPECT_TRUE(can_comment(Role::Editor));
}

TEST(AccessRules, CommenterCapabilities) {
    EXPECT_FALSE(can_invite(Role::Commenter));
    EXPECT_FALSE(can_change_roles(Role::Commenter));
    EXPECT_FALSE(can_edit_structure(Role::Commenter));
    EXPECT_FALSE(can_edit_node_content(Role::Commenter));
    EXPECT_TRUE(can_comment(Role::Commenter));
}

TEST(AccessRules, ViewerCapabilities) {
    EXPECT_FALSE(can_invite(Role::Viewer));
    EXPECT_FALSE(can_change_roles(Role::Viewer));
    EXPECT_FALSE(can_edit_structure(Role::Viewer));
    EXPECT_FALSE(can_edit_node_content(Role::Viewer));
    EXPECT_FALSE(can_comment(Role::Viewer));
}

TEST(AccessRules, MembershipRoleExcludesOwner) {
    EXPECT_TRUE(is_membership_role(Role::Editor));
    EXPECT_TRUE(is_membership_role(Role::Commenter));
    EXPECT_TRUE(is_membership_role(Role::Viewer));
    EXPECT_FALSE(is_membership_role(Role::Owner));
}

TEST(AccessRules, EffectiveRole) {
    EXPECT_EQ(effective_role("a", "a", std::nullopt), Role::Owner);
    EXPECT_EQ(effective_role("a", "o", Role::Editor), Role::Editor);
    EXPECT_EQ(effective_role("a", "o", std::nullopt), Role::Viewer);
}
