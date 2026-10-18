#include <doctest/doctest.h>

#include "protocol/message.h"

using namespace newsdesk;
using namespace newsdesk::protocol;
using namespace std::chrono_literals;

TEST_CASE("ParseIncoming parses a hello message") {
    auto parsed = ParseIncoming(R"({"type":"hello","client_id":"alice","last_seq":5})");
    REQUIRE(parsed.has_value());
    CHECK(parsed->type == IncomingType::kHello);
    REQUIRE(parsed->hello.has_value());
    CHECK(parsed->hello->client_id == "alice");
    CHECK(parsed->hello->last_seq == 5);
}

TEST_CASE("ParseIncoming parses a note message") {
    auto parsed = ParseIncoming(
        R"({"type":"note","note_id":"n1","rubric":"local","topic":"fire","event_time":1000})");
    REQUIRE(parsed.has_value());
    CHECK(parsed->type == IncomingType::kNote);
    REQUIRE(parsed->note.has_value());
    CHECK(parsed->note->note_id == "n1");
    CHECK(parsed->note->rubric == "local");
    CHECK(parsed->note->topic == "fire");
}

TEST_CASE("ParseIncoming parses an ack message") {
    auto parsed = ParseIncoming(R"({"type":"ack","seq":7})");
    REQUIRE(parsed.has_value());
    CHECK(parsed->type == IncomingType::kAck);
    REQUIRE(parsed->ack.has_value());
    CHECK(parsed->ack->seq == 7);
}

TEST_CASE("ParseIncoming parses a stats message without a target") {
    auto parsed = ParseIncoming(R"({"type":"stats"})");
    REQUIRE(parsed.has_value());
    CHECK(parsed->type == IncomingType::kStats);
    REQUIRE(parsed->stats.has_value());
    CHECK_FALSE(parsed->stats->target_client_id.has_value());
}

TEST_CASE("ParseIncoming parses a stats message with a target client") {
    auto parsed = ParseIncoming(R"({"type":"stats","client_id":"bob"})");
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->stats.has_value());
    REQUIRE(parsed->stats->target_client_id.has_value());
    CHECK(*parsed->stats->target_client_id == "bob");
}

TEST_CASE("ParseIncoming parses a bye message") {
    auto parsed = ParseIncoming(R"({"type":"bye"})");
    REQUIRE(parsed.has_value());
    CHECK(parsed->type == IncomingType::kBye);
}

TEST_CASE("ParseIncoming rejects malformed JSON") {
    CHECK_FALSE(ParseIncoming("not json").has_value());
}

TEST_CASE("ParseIncoming rejects a message without a type field") {
    CHECK_FALSE(ParseIncoming(R"({"client_id":"alice"})").has_value());
}

TEST_CASE("ParseIncoming rejects an unknown type") {
    CHECK_FALSE(ParseIncoming(R"({"type":"mystery"})").has_value());
}

TEST_CASE("ParseIncoming rejects a note missing a required field") {
    CHECK_FALSE(ParseIncoming(R"({"type":"note","note_id":"n1","rubric":"local"})").has_value());
}

TEST_CASE("ParseIncoming rejects a field with the wrong JSON type") {
    CHECK_FALSE(
        ParseIncoming(R"({"type":"hello","client_id":"alice","last_seq":"five"})").has_value());
}

TEST_CASE("Serializing and parsing a hello message round-trips") {
    HelloMessage hello{"alice", 3};
    auto parsed = ParseIncoming(Serialize(hello));
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->hello.has_value());
    CHECK(parsed->hello->client_id == "alice");
    CHECK(parsed->hello->last_seq == 3);
}

TEST_CASE("Serializing and parsing a note message round-trips the event_time") {
    NoteMessage note;
    note.note_id = "n1";
    note.rubric = "local";
    note.topic = "fire";
    note.event_time = TimePoint{} + 1000h;

    auto parsed = ParseIncoming(Serialize(note));
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->note.has_value());
    CHECK(parsed->note->event_time == note.event_time);
}

TEST_CASE("Serializing and parsing an ack message round-trips") {
    AckMessage ack{42};
    auto parsed = ParseIncoming(Serialize(ack));
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->ack.has_value());
    CHECK(parsed->ack->seq == 42);
}

TEST_CASE("Serializing and parsing a stats request round-trips a target client") {
    StatsRequestMessage request;
    request.target_client_id = "carol";

    auto parsed = ParseIncoming(Serialize(request));
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->stats.has_value());
    REQUIRE(parsed->stats->target_client_id.has_value());
    CHECK(*parsed->stats->target_client_id == "carol");
}

TEST_CASE("Serializing and parsing a bye message round-trips") {
    auto parsed = ParseIncoming(Serialize(ByeMessage{}));
    REQUIRE(parsed.has_value());
    CHECK(parsed->type == IncomingType::kBye);
}

TEST_CASE("Serialize produces the expected fields for a welcome message") {
    std::string json = Serialize(WelcomeMessage{"alice", Role::kEditor});
    CHECK(json.find("\"type\":\"welcome\"") != std::string::npos);
    CHECK(json.find("\"client_id\":\"alice\"") != std::string::npos);
    CHECK(json.find("\"role\":\"editor\"") != std::string::npos);
}

TEST_CASE("Serialize produces the expected fields for an error message") {
    std::string json = Serialize(ErrorMessage{"unknown_client", "no such client"});
    CHECK(json.find("\"type\":\"error\"") != std::string::npos);
    CHECK(json.find("\"code\":\"unknown_client\"") != std::string::npos);
}

TEST_CASE("Serialize includes the reason only for a rejected receipt") {
    std::string accepted = Serialize(Receipt{"n1", ReceiptStatus::kAccepted, ""});
    CHECK(accepted.find("\"status\":\"accepted\"") != std::string::npos);
    CHECK(accepted.find("reason") == std::string::npos);

    std::string rejected = Serialize(Receipt{"n1", ReceiptStatus::kRejected, "empty_story"});
    CHECK(rejected.find("\"status\":\"rejected\"") != std::string::npos);
    CHECK(rejected.find("\"reason\":\"empty_story\"") != std::string::npos);
}

TEST_CASE("Serialize produces the expected fields for a congrats payload") {
    CongratsPayload payload{5, StoryKey{"local", "fire"}, 2, "text"};
    std::string json = Serialize(payload);
    CHECK(json.find("\"type\":\"congrats\"") != std::string::npos);
    CHECK(json.find("\"seq\":5") != std::string::npos);
    CHECK(json.find("\"rank\":2") != std::string::npos);
    CHECK(json.find("\"rubric\":\"local\"") != std::string::npos);
    CHECK(json.find("\"topic\":\"fire\"") != std::string::npos);
}

TEST_CASE("ParseServerMessage round-trips a congrats payload including seq") {
    CongratsPayload payload{9, StoryKey{"local", "fire"}, 1, "text"};
    auto parsed = ParseServerMessage(Serialize(payload));
    REQUIRE(parsed.has_value());
    CHECK(parsed->type == OutgoingType::kCongrats);
    REQUIRE(parsed->congrats.has_value());
    CHECK(parsed->congrats->seq == 9);
    CHECK(parsed->congrats->rank == 1);
    CHECK(parsed->congrats->story.rubric == "local");
}

TEST_CASE("ParseServerMessage round-trips a receipt, including the reason when rejected") {
    auto parsed =
        ParseServerMessage(Serialize(Receipt{"n1", ReceiptStatus::kRejected, "empty_story"}));
    REQUIRE(parsed.has_value());
    CHECK(parsed->type == OutgoingType::kReceipt);
    REQUIRE(parsed->receipt.has_value());
    CHECK(parsed->receipt->status == ReceiptStatus::kRejected);
    CHECK(parsed->receipt->reason == "empty_story");
}

TEST_CASE("ParseServerMessage round-trips a welcome message") {
    auto parsed = ParseServerMessage(Serialize(WelcomeMessage{"alice", Role::kEditor}));
    REQUIRE(parsed.has_value());
    CHECK(parsed->type == OutgoingType::kWelcome);
    REQUIRE(parsed->welcome.has_value());
    CHECK(parsed->welcome->client_id == "alice");
    CHECK(parsed->welcome->role == Role::kEditor);
}

TEST_CASE("ParseServerMessage round-trips a stats response with multiple clients") {
    StatsResponseMessage response;
    response.stats.push_back(ClientStats{"alice", 2, 1, 0.5, 2, 1, 1.0});
    response.stats.push_back(ClientStats{"bob", 1, 0, 0.0, 1, 0, 0.0});

    auto parsed = ParseServerMessage(Serialize(response));
    REQUIRE(parsed.has_value());
    CHECK(parsed->type == OutgoingType::kStatsResponse);
    REQUIRE(parsed->stats_response.has_value());
    REQUIRE(parsed->stats_response->stats.size() == 2);
    CHECK(parsed->stats_response->stats[0].client_id == "alice");
    CHECK(parsed->stats_response->stats[1].client_id == "bob");
}

TEST_CASE("ParseServerMessage rejects malformed JSON") {
    CHECK_FALSE(ParseServerMessage("not json").has_value());
}

TEST_CASE("Serialize produces an array with every client for a stats response") {
    StatsResponseMessage response;
    response.stats.push_back(ClientStats{"alice", 2, 1, 0.5, 2, 1, 1.0});
    response.stats.push_back(ClientStats{"bob", 1, 0, 0.0, 1, 0, 0.0});

    std::string json = Serialize(response);
    CHECK(json.find("\"type\":\"stats\"") != std::string::npos);
    CHECK(json.find("\"client_id\":\"alice\"") != std::string::npos);
    CHECK(json.find("\"client_id\":\"bob\"") != std::string::npos);
}
