#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "core/clock.h"
#include "core/registry.h"
#include "core/stats.h"
#include "core/story.h"

namespace newsdesk {

enum class ReceiptStatus {
    kAccepted,
    kDuplicate,
    kRejected,
};

struct Receipt {
    std::string note_id;
    ReceiptStatus status;
    std::string reason;
};

struct CongratsMessage {
    std::string client_id;
    StoryKey story;
    int rank;
    std::string text;
};

struct NoteOutcome {
    Receipt receipt;
    bool story_published = false;
    std::vector<CongratsMessage> congrats;
};

struct EngineConfig {
    double threshold;
    int congratulated_k;
    Duration half_life;
};

class Engine {
public:
    Engine(Registry& registry, EngineConfig config);

    NoteOutcome HandleNote(const std::string& client_id, const std::string& note_id,
                           const std::string& rubric, const std::string& topic,
                           TimePoint event_time, TimePoint received_time);

    ClientStats GetStats(const std::string& client_id) const;
    std::vector<ClientStats> GetAllStats() const;

private:
    Registry& registry_;
    EngineConfig config_;

    std::unordered_map<StoryKey, Story, StoryKeyHash> stories_;
    std::unordered_map<std::string, std::unordered_map<std::string, Receipt>> seen_notes_;
    StatsTracker stats_;
};

}  // namespace newsdesk
