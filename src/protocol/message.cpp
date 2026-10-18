#include "protocol/message.h"

#include <nlohmann/json.hpp>

namespace newsdesk::protocol {

namespace {

int64_t ToMillis(TimePoint time) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count();
}

TimePoint FromMillis(int64_t millis) {
    return TimePoint{} + std::chrono::milliseconds(millis);
}

std::string RoleToString(Role role) { return role == Role::kEditor ? "editor" : "reporter"; }

std::string ReceiptStatusToString(ReceiptStatus status) {
    switch (status) {
        case ReceiptStatus::kAccepted:
            return "accepted";
        case ReceiptStatus::kDuplicate:
            return "duplicate";
        case ReceiptStatus::kRejected:
            return "rejected";
    }
    return "rejected";
}

}  // namespace

std::optional<ParsedMessage> ParseIncoming(const std::string& line) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(line);
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }

    if (!j.is_object() || !j.contains("type") || !j["type"].is_string()) {
        return std::nullopt;
    }
    std::string type = j["type"].get<std::string>();

    try {
        if (type == "hello") {
            ParsedMessage message;
            message.type = IncomingType::kHello;
            HelloMessage hello;
            hello.client_id = j.at("client_id").get<std::string>();
            hello.last_seq = j.at("last_seq").get<uint64_t>();
            message.hello = hello;
            return message;
        }
        if (type == "note") {
            ParsedMessage message;
            message.type = IncomingType::kNote;
            NoteMessage note;
            note.note_id = j.at("note_id").get<std::string>();
            note.rubric = j.at("rubric").get<std::string>();
            note.topic = j.at("topic").get<std::string>();
            note.event_time = FromMillis(j.at("event_time").get<int64_t>());
            message.note = note;
            return message;
        }
        if (type == "ack") {
            ParsedMessage message;
            message.type = IncomingType::kAck;
            AckMessage ack;
            ack.seq = j.at("seq").get<uint64_t>();
            message.ack = ack;
            return message;
        }
        if (type == "stats") {
            ParsedMessage message;
            message.type = IncomingType::kStats;
            StatsRequestMessage stats;
            if (j.contains("client_id") && !j["client_id"].is_null()) {
                stats.target_client_id = j["client_id"].get<std::string>();
            }
            message.stats = stats;
            return message;
        }
        if (type == "bye") {
            ParsedMessage message;
            message.type = IncomingType::kBye;
            return message;
        }
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }

    return std::nullopt;
}

std::string Serialize(const HelloMessage& message) {
    nlohmann::json j;
    j["type"] = "hello";
    j["client_id"] = message.client_id;
    j["last_seq"] = message.last_seq;
    return j.dump();
}

std::string Serialize(const NoteMessage& message) {
    nlohmann::json j;
    j["type"] = "note";
    j["note_id"] = message.note_id;
    j["rubric"] = message.rubric;
    j["topic"] = message.topic;
    j["event_time"] = ToMillis(message.event_time);
    return j.dump();
}

std::string Serialize(const AckMessage& message) {
    nlohmann::json j;
    j["type"] = "ack";
    j["seq"] = message.seq;
    return j.dump();
}

std::string Serialize(const StatsRequestMessage& message) {
    nlohmann::json j;
    j["type"] = "stats";
    if (message.target_client_id.has_value()) {
        j["client_id"] = *message.target_client_id;
    }
    return j.dump();
}

std::string Serialize(const ByeMessage&) {
    nlohmann::json j;
    j["type"] = "bye";
    return j.dump();
}

std::string Serialize(const WelcomeMessage& message) {
    nlohmann::json j;
    j["type"] = "welcome";
    j["client_id"] = message.client_id;
    j["role"] = RoleToString(message.role);
    return j.dump();
}

std::string Serialize(const ErrorMessage& message) {
    nlohmann::json j;
    j["type"] = "error";
    j["code"] = message.code;
    j["message"] = message.message;
    return j.dump();
}

std::string Serialize(const Receipt& message) {
    nlohmann::json j;
    j["type"] = "receipt";
    j["note_id"] = message.note_id;
    j["status"] = ReceiptStatusToString(message.status);
    if (message.status == ReceiptStatus::kRejected) {
        j["reason"] = message.reason;
    }
    return j.dump();
}

std::string Serialize(const CongratsPayload& message) {
    nlohmann::json j;
    j["type"] = "congrats";
    j["seq"] = message.seq;
    j["story"] = {{"rubric", message.story.rubric}, {"topic", message.story.topic}};
    j["rank"] = message.rank;
    j["text"] = message.text;
    return j.dump();
}

std::string Serialize(const StatsResponseMessage& message) {
    nlohmann::json j;
    j["type"] = "stats";
    nlohmann::json entries = nlohmann::json::array();
    for (const ClientStats& stats : message.stats) {
        entries.push_back({
            {"client_id", stats.client_id},
            {"accepted", stats.accepted},
            {"published", stats.published},
            {"share", stats.share},
            {"stories", stats.stories},
            {"scoops", stats.scoops},
            {"avg_rank", stats.avg_rank},
        });
    }
    j["stats"] = entries;
    return j.dump();
}

}  // namespace newsdesk::protocol
