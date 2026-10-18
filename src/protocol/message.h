#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/clock.h"
#include "core/engine.h"
#include "core/registry.h"
#include "core/stats.h"
#include "core/story.h"

namespace newsdesk::protocol {

// client -> server.
enum class IncomingType {
    kHello,
    kNote,
    kAck,
    kStats,
    kBye,
};

struct HelloMessage {
    std::string client_id;
    uint64_t last_seq = 0;
};

struct NoteMessage {
    std::string note_id;
    std::string rubric;
    std::string topic;
    TimePoint event_time;
};

struct AckMessage {
    uint64_t seq = 0;
};

struct StatsRequestMessage {
    std::optional<std::string> target_client_id;
};

struct ByeMessage {};

struct ParsedMessage {
    IncomingType type;
    std::optional<HelloMessage> hello;
    std::optional<NoteMessage> note;
    std::optional<AckMessage> ack;
    std::optional<StatsRequestMessage> stats;
};

std::optional<ParsedMessage> ParseIncoming(const std::string& line);

std::string Serialize(const HelloMessage& message);
std::string Serialize(const NoteMessage& message);
std::string Serialize(const AckMessage& message);
std::string Serialize(const StatsRequestMessage& message);
std::string Serialize(const ByeMessage& message);

// server -> client.
struct WelcomeMessage {
    std::string client_id;
    Role role;
};

struct ErrorMessage {
    std::string code;
    std::string message;
};

struct CongratsPayload {
    uint64_t seq = 0;
    StoryKey story;
    int rank = 0;
    std::string text;
};

struct StatsResponseMessage {
    std::vector<ClientStats> stats;
};

// Для каждого сообщения возвращает JSON text
std::string Serialize(const WelcomeMessage& message);
std::string Serialize(const ErrorMessage& message);
std::string Serialize(const Receipt& message);
std::string Serialize(const CongratsPayload& message);
std::string Serialize(const StatsResponseMessage& message);

enum class OutgoingType {
    kWelcome,
    kError,
    kReceipt,
    kCongrats,
    kStatsResponse,
};

struct ParsedServerMessage {
    OutgoingType type;
    std::optional<WelcomeMessage> welcome;
    std::optional<ErrorMessage> error;
    std::optional<Receipt> receipt;
    std::optional<CongratsPayload> congrats;
    std::optional<StatsResponseMessage> stats_response;
};

std::optional<ParsedServerMessage> ParseServerMessage(const std::string& line);

}  // namespace newsdesk::protocol
