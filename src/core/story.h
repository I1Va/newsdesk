#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/clock.h"

namespace newsdesk {

struct StoryKey {
    std::string rubric;
    std::string topic;

    bool operator==(const StoryKey& other) const;
};

struct StoryKeyHash {
    size_t operator()(const StoryKey& key) const;
};

StoryKey NormalizeKey(const std::string& rubric, const std::string& topic);

struct Contribution {
    std::string client_id;
    double weight;
    TimePoint anchor_time;
};

struct Submitter {
    std::string client_id;
    TimePoint received_time;
};

class Story {
public:
    explicit Story(StoryKey key);

    const StoryKey& Key() const;
    bool HasContributionFrom(const std::string& client_id) const;
    void AddContribution(const std::string& client_id, double weight, TimePoint anchor_time,
                          TimePoint received_time);
    double CurrentTrust(TimePoint now, Duration half_life) const;

    bool IsPublished() const;
    void Publish(TimePoint published_at);
    TimePoint PublishedAt() const;

    const std::vector<Submitter>& Submitters() const;
    const std::vector<Contribution>& Contributions() const;

private:
    StoryKey key_;
    std::vector<Contribution> contributions_;
    std::vector<Submitter> submitters_;
    bool published_ = false;
    TimePoint published_at_;
};

}  // namespace newsdesk
