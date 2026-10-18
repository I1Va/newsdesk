#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/story.h"

namespace newsdesk {

struct ClientStats {
    std::string client_id;
    uint64_t accepted = 0;
    uint64_t published = 0;
    double share = 0.0;
    uint64_t stories = 0;
    uint64_t scoops = 0;
    double avg_rank = 0.0;
};

class StatsTracker {
public:
    void RecordAccepted(const std::string& client_id, const StoryKey& story_key);
    void RecordPublished(const std::string& client_id, int rank);

    ClientStats Get(const std::string& client_id) const;
    std::vector<ClientStats> GetAll() const;

private:
    struct Entry {
        uint64_t accepted = 0;
        uint64_t published = 0;
        uint64_t scoops = 0;
        std::vector<int> ranks;
        std::unordered_set<StoryKey, StoryKeyHash> stories;
    };

    std::unordered_map<std::string, Entry> entries_;
};

}  // namespace newsdesk
