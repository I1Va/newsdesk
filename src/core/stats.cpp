#include "core/stats.h"

namespace newsdesk {

void StatsTracker::RecordAccepted(const std::string& client_id, const StoryKey& story_key) {
    Entry& entry = entries_[client_id];
    entry.accepted += 1;
    entry.stories.insert(story_key);
}

void StatsTracker::RecordPublished(const std::string& client_id, int rank) {
    Entry& entry = entries_[client_id];
    entry.published += 1;
    entry.ranks.push_back(rank);
    if (rank == 1) {
        entry.scoops += 1;
    }
}

ClientStats StatsTracker::Get(const std::string& client_id) const {
    ClientStats result;
    result.client_id = client_id;

    auto it = entries_.find(client_id);
    if (it == entries_.end()) {
        return result;
    }

    const Entry& entry = it->second;
    result.accepted = entry.accepted;
    result.published = entry.published;
    result.share = entry.accepted == 0
                        ? 0.0
                        : static_cast<double>(entry.published) / static_cast<double>(entry.accepted);
    result.stories = entry.stories.size();
    result.scoops = entry.scoops;

    if (!entry.ranks.empty()) {
        double rank_sum = 0.0;
        for (int rank : entry.ranks) {
            rank_sum += rank;
        }
        result.avg_rank = rank_sum / static_cast<double>(entry.ranks.size());
    }

    return result;
}

std::vector<ClientStats> StatsTracker::GetAll() const {
    std::vector<ClientStats> result;
    result.reserve(entries_.size());
    for (const auto& [client_id, entry] : entries_) {
        result.push_back(Get(client_id));
    }
    return result;
}

}  // namespace newsdesk
