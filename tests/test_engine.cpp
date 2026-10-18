#include <doctest/doctest.h>

#include "core/engine.h"

using namespace newsdesk;
using namespace std::chrono_literals;

namespace {

const TimePoint kBase = TimePoint{} + 1000h;

Registry MakeRegistry() {
    Registry registry;
    registry.Add(ClientInfo{"c1", 0.4, Role::kReporter});
    registry.Add(ClientInfo{"c2", 0.4, Role::kReporter});
    registry.Add(ClientInfo{"c3", 0.4, Role::kReporter});
    return registry;
}

}  // namespace

TEST_CASE("HandleNote rejects a note from an unregistered client") {
    Registry registry = MakeRegistry();
    Engine engine(registry, EngineConfig{0.75, 2, 6h});

    NoteOutcome outcome = engine.HandleNote("ghost", "n1", "local", "fire", kBase, kBase);
    CHECK(outcome.receipt.status == ReceiptStatus::kRejected);
    CHECK(outcome.receipt.reason == "unknown_client");
}

TEST_CASE("HandleNote rejects an empty story key after normalization") {
    Registry registry = MakeRegistry();
    Engine engine(registry, EngineConfig{0.75, 2, 6h});

    NoteOutcome outcome = engine.HandleNote("c1", "n1", "   ", "topic", kBase, kBase);
    CHECK(outcome.receipt.status == ReceiptStatus::kRejected);
    CHECK(outcome.receipt.reason == "empty_story");
}

TEST_CASE("HandleNote returns duplicate for a repeated accepted note_id") {
    Registry registry = MakeRegistry();
    Engine engine(registry, EngineConfig{0.75, 2, 6h});

    engine.HandleNote("c1", "n1", "local", "fire", kBase, kBase);
    NoteOutcome outcome = engine.HandleNote("c1", "n1", "local", "fire", kBase, kBase);

    CHECK(outcome.receipt.status == ReceiptStatus::kDuplicate);
    CHECK(engine.GetStats("c1").accepted == 1);
}

TEST_CASE("HandleNote replays the same rejection for a repeated rejected note_id") {
    Registry registry = MakeRegistry();
    Engine engine(registry, EngineConfig{0.75, 2, 6h});

    engine.HandleNote("ghost", "n1", "local", "fire", kBase, kBase);
    NoteOutcome outcome = engine.HandleNote("ghost", "n1", "topic that differs", "x", kBase, kBase);

    CHECK(outcome.receipt.status == ReceiptStatus::kRejected);
    CHECK(outcome.receipt.reason == "unknown_client");
}

TEST_CASE("A second note from the same client does not add weight or change order") {
    Registry registry = MakeRegistry();
    Engine engine(registry, EngineConfig{0.5, 2, 6h});

    NoteOutcome first = engine.HandleNote("c1", "n1", "local", "fire", kBase, kBase);
    NoteOutcome second = engine.HandleNote("c1", "n2", "local", "fire", kBase, kBase + 1h);

    CHECK(first.receipt.status == ReceiptStatus::kAccepted);
    CHECK(second.receipt.status == ReceiptStatus::kAccepted);
    CHECK_FALSE(second.story_published);
    CHECK(engine.GetStats("c1").accepted == 2);
    CHECK(engine.GetStats("c1").stories == 1);
}

TEST_CASE("Reaching the threshold publishes the story and congratulates the first K") {
    Registry registry;
    registry.Add(ClientInfo{"c1", 0.3, Role::kReporter});
    registry.Add(ClientInfo{"c2", 0.3, Role::kReporter});
    registry.Add(ClientInfo{"c3", 0.3, Role::kReporter});
    Engine engine(registry, EngineConfig{0.75, 2, 6h});

    engine.HandleNote("c1", "n1", "local", "fire", kBase, kBase);
    NoteOutcome second = engine.HandleNote("c2", "n2", "local", "fire", kBase, kBase);
    NoteOutcome third = engine.HandleNote("c3", "n3", "local", "fire", kBase, kBase);

    CHECK_FALSE(second.story_published);
    CHECK(third.story_published);
    REQUIRE(third.congrats.size() == 2);
    CHECK(third.congrats[0].client_id == "c1");
    CHECK(third.congrats[0].rank == 1);
    CHECK(third.congrats[1].client_id == "c2");
    CHECK(third.congrats[1].rank == 2);
}

TEST_CASE("A note for an already published story is accepted without a vote") {
    Registry registry = MakeRegistry();
    Engine engine(registry, EngineConfig{0.4, 2, 6h});

    engine.HandleNote("c1", "n1", "local", "fire", kBase, kBase);
    registry.Add(ClientInfo{"c4", 0.4, Role::kReporter});
    NoteOutcome late = engine.HandleNote("c4", "n4", "local", "fire", kBase, kBase + 1h);

    CHECK(late.receipt.status == ReceiptStatus::kAccepted);
    CHECK_FALSE(late.story_published);
    CHECK(engine.GetStats("c4").accepted == 1);
    CHECK(engine.GetStats("c4").published == 0);
}

TEST_CASE("A note's weight decays from its event_time, not from received_time") {
    Registry registry;
    registry.Add(ClientInfo{"c1", 1.0, Role::kReporter});
    Engine engine(registry, EngineConfig{0.4, 1, 6h});

    NoteOutcome outcome =
        engine.HandleNote("c1", "n1", "local", "fire", kBase - 12h, kBase);

    CHECK(outcome.receipt.status == ReceiptStatus::kAccepted);
    CHECK_FALSE(outcome.story_published);
}

TEST_CASE("An event_time in the future is clamped to received_time") {
    Registry registry;
    registry.Add(ClientInfo{"c1", 0.3, Role::kReporter});
    registry.Add(ClientInfo{"c2", 0.3, Role::kReporter});
    Engine engine(registry, EngineConfig{0.5, 1, 6h});

    engine.HandleNote("c1", "n1", "local", "fire", kBase + 100h, kBase);
    NoteOutcome second =
        engine.HandleNote("c2", "n2", "local", "fire", kBase + 50h, kBase + 50h);

    CHECK_FALSE(second.story_published);
}
