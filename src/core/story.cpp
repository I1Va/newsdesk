#include "core/story.h"

#include <cctype>

#include "core/trust.h"

namespace newsdesk {

namespace {

std::string NormalizeField(const std::string& field) {
    size_t start = 0;
    size_t end = field.size();
    while (start < end && std::isspace(static_cast<unsigned char>(field[start]))) {
        ++start;
    }
    while (end > start && std::isspace(static_cast<unsigned char>(field[end - 1]))) {
        --end;
    }

    std::string result;
    bool in_space = false;
    for (size_t i = start; i < end; ++i) {
        char c = field[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!in_space) {
                result.push_back(' ');
                in_space = true;
            }
        } else {
            result.push_back(c);
            in_space = false;
        }
    }
    return result;
}

}  // namespace

bool StoryKey::operator==(const StoryKey& other) const {
    return rubric == other.rubric && topic == other.topic;
}

size_t StoryKeyHash::operator()(const StoryKey& key) const {
    size_t rubric_hash = std::hash<std::string>{}(key.rubric);
    size_t topic_hash = std::hash<std::string>{}(key.topic);
    return rubric_hash ^ (topic_hash << 1);
}

StoryKey NormalizeKey(const std::string& rubric, const std::string& topic) {
    return StoryKey{NormalizeField(rubric), NormalizeField(topic)};
}

Story::Story(StoryKey key) : key_(std::move(key)) {}

const StoryKey& Story::Key() const { return key_; }

bool Story::HasContributionFrom(const std::string& client_id) const {
    for (const auto& contribution : contributions_) {
        if (contribution.client_id == client_id) {
            return true;
        }
    }
    return false;
}

void Story::AddContribution(const std::string& client_id, double weight, TimePoint anchor_time,
                             TimePoint received_time) {
    contributions_.push_back(Contribution{client_id, weight, anchor_time});
    submitters_.push_back(Submitter{client_id, received_time});
}

double Story::CurrentTrust(TimePoint now, Duration half_life) const {
    double total = 0.0;
    for (const auto& contribution : contributions_) {
        total += Decay(contribution.weight, contribution.anchor_time, now, half_life);
    }
    return total;
}

bool Story::IsPublished() const { return published_; }

void Story::Publish(TimePoint published_at) {
    published_ = true;
    published_at_ = published_at;
}

TimePoint Story::PublishedAt() const { return published_at_; }

const std::vector<Submitter>& Story::Submitters() const { return submitters_; }

const std::vector<Contribution>& Story::Contributions() const { return contributions_; }

}  // namespace newsdesk
