#include "../include/mind_map/versioning/revision_check.hpp"

#include "mind_map/validation/errors/errors.hpp"

#include <gtest/gtest.h>

using namespace mind_map;
using namespace mind_map::versioning;

TEST(RevisionCheck, OptionalMatch) {
    EXPECT_EQ(check_revision(std::nullopt, 99), StatusCode::Ok);
    EXPECT_EQ(check_revision(Version{5}, Version{5}), StatusCode::Ok);
    EXPECT_EQ(check_revision(Version{5}, Version{4}), StatusCode::VersionMismatch);
}
