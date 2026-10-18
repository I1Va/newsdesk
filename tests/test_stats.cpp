#include <doctest/doctest.h>

#include "core/stats.h"

using namespace newsdesk;

TEST_CASE("Get for an unknown client returns zeroed stats") {
    StatsTracker tracker;
    ClientStats stats = tracker.Get("ghost");
    CHECK(stats.client_id == "ghost");
    CHECK(stats.accepted == 0);
    CHECK(stats.published == 0);
    CHECK(stats.share == doctest::Approx(0.0));
    CHECK(stats.stories == 0);
    CHECK(stats.scoops == 0);
    CHECK(stats.avg_rank == doctest::Approx(0.0));
}

TEST_CASE("RecordAccepted counts notes and distinct stories") {
    StatsTracker tracker;
    tracker.RecordAccepted("c1", StoryKey{"local", "fire"});
    tracker.RecordAccepted("c1", StoryKey{"local", "fire"});
    tracker.RecordAccepted("c1", StoryKey{"local", "flood"});

    ClientStats stats = tracker.Get("c1");
    CHECK(stats.accepted == 3);
    CHECK(stats.stories == 2);
}

TEST_CASE("RecordPublished tracks published count, scoops and avg_rank") {
    StatsTracker tracker;
    tracker.RecordAccepted("c1", StoryKey{"local", "fire"});
    tracker.RecordAccepted("c1", StoryKey{"local", "flood"});
    tracker.RecordPublished("c1", 1);
    tracker.RecordPublished("c1", 3);

    ClientStats stats = tracker.Get("c1");
    CHECK(stats.published == 2);
    CHECK(stats.scoops == 1);
    CHECK(stats.avg_rank == doctest::Approx(2.0));
    CHECK(stats.share == doctest::Approx(1.0));
}

TEST_CASE("GetAll returns an entry for every client that was recorded") {
    StatsTracker tracker;
    tracker.RecordAccepted("c1", StoryKey{"local", "fire"});
    tracker.RecordAccepted("c2", StoryKey{"local", "fire"});

    auto all = tracker.GetAll();
    CHECK(all.size() == 2);
}
