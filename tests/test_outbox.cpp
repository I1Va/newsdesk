#include <doctest/doctest.h>

#include "core/outbox.h"

using namespace newsdesk;

TEST_CASE("Enqueue assigns increasing seq numbers starting at 1") {
    Outbox outbox(10);
    CHECK(outbox.Enqueue("a") == 1);
    CHECK(outbox.Enqueue("b") == 2);
    CHECK(outbox.Size() == 2);
}

TEST_CASE("MessagesAfter returns only entries newer than last_seq") {
    Outbox outbox(10);
    outbox.Enqueue("a");
    outbox.Enqueue("b");
    outbox.Enqueue("c");

    auto messages = outbox.MessagesAfter(1);
    REQUIRE(messages.size() == 2);
    CHECK(messages[0].payload == "b");
    CHECK(messages[1].payload == "c");

    CHECK(outbox.MessagesAfter(3).empty());
    CHECK(outbox.MessagesAfter(0).size() == 3);
}

TEST_CASE("Ack removes every entry up to and including the given seq") {
    Outbox outbox(10);
    outbox.Enqueue("a");
    outbox.Enqueue("b");
    outbox.Enqueue("c");

    outbox.Ack(2);
    CHECK(outbox.Size() == 1);
    CHECK(outbox.MessagesAfter(0)[0].payload == "c");
}

TEST_CASE("IsFull reflects the configured max size") {
    Outbox outbox(2);
    CHECK_FALSE(outbox.IsFull());
    outbox.Enqueue("a");
    outbox.Enqueue("b");
    CHECK(outbox.IsFull());
}
