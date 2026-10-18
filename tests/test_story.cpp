#include <doctest/doctest.h>

#include <unordered_map>

#include "core/story.h"

using namespace newsdesk;
using namespace std::chrono_literals;

namespace {

const TimePoint kBase = TimePoint{} + 1000h;

}  // namespace

TEST_CASE("NormalizeKey trims edges and collapses repeated whitespace") {
    StoryKey key = NormalizeKey("  Politics  ", "Mayor   resigns\tsuddenly ");
    CHECK(key.rubric == "Politics");
    CHECK(key.topic == "Mayor resigns suddenly");
}

TEST_CASE("NormalizeKey keeps the original case") {
    StoryKey key = NormalizeKey("Local", "City Hall Fire");
    CHECK(key.topic == "City Hall Fire");
}

TEST_CASE("StoryKey equality and hashing work as an unordered_map key") {
    std::unordered_map<StoryKey, int, StoryKeyHash> map;
    map[StoryKey{"local", "fire"}] = 1;
    CHECK(map.at(StoryKey{"local", "fire"}) == 1);
    CHECK(map.find(StoryKey{"local", "flood"}) == map.end());
}

TEST_CASE("Story starts with no contributions and unpublished") {
    Story story(StoryKey{"local", "fire"});
    CHECK_FALSE(story.HasContributionFrom("c1"));
    CHECK_FALSE(story.IsPublished());
    CHECK(story.CurrentTrust(kBase, 6h) == doctest::Approx(0.0));
}

TEST_CASE("AddContribution records the submitter in arrival order") {
    Story story(StoryKey{"local", "fire"});
    story.AddContribution("c1", 0.5, kBase, kBase);
    story.AddContribution("c2", 0.3, kBase, kBase + 1h);

    CHECK(story.HasContributionFrom("c1"));
    CHECK(story.HasContributionFrom("c2"));
    CHECK_FALSE(story.HasContributionFrom("c3"));

    const auto& submitters = story.Submitters();
    REQUIRE(submitters.size() == 2);
    CHECK(submitters[0].client_id == "c1");
    CHECK(submitters[1].client_id == "c2");
}

TEST_CASE("CurrentTrust sums the decayed weight of every contribution") {
    Story story(StoryKey{"local", "fire"});
    story.AddContribution("c1", 0.5, kBase, kBase);
    story.AddContribution("c2", 0.3, kBase, kBase);

    CHECK(story.CurrentTrust(kBase, 6h) == doctest::Approx(0.8));
    CHECK(story.CurrentTrust(kBase + 6h, 6h) == doctest::Approx(0.4));
}

TEST_CASE("Publish sets the published flag and timestamp") {
    Story story(StoryKey{"local", "fire"});
    CHECK_FALSE(story.IsPublished());

    story.Publish(kBase);
    CHECK(story.IsPublished());
    CHECK(story.PublishedAt() == kBase);
}
