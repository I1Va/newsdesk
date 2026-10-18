#include "core/engine.h"

namespace newsdesk {

Engine::Engine(Registry& registry, EngineConfig config)
    : registry_(registry), config_(config) {}

NoteOutcome Engine::HandleNote(const std::string& client_id, const std::string& note_id,
                                const std::string& rubric, const std::string& topic,
                                TimePoint event_time, TimePoint received_time) {
    NoteOutcome outcome;

    // Проверяем на повтор
    auto& client_notes = seen_notes_[client_id];
    auto found = client_notes.find(note_id);
    if (found != client_notes.end()) {
        const Receipt& stored = found->second;
        outcome.receipt = stored.status == ReceiptStatus::kRejected
                               ? stored
                               : Receipt{note_id, ReceiptStatus::kDuplicate, ""};
        return outcome;
    }

    const ClientInfo* client = registry_.Find(client_id);
    if (client == nullptr) {
        Receipt receipt{note_id, ReceiptStatus::kRejected, "unknown_client"};
        client_notes[note_id] = receipt;
        outcome.receipt = receipt;
        return outcome;
    }

    StoryKey key = NormalizeKey(rubric, topic);
    if (key.rubric.empty() || key.topic.empty()) {
        Receipt receipt{note_id, ReceiptStatus::kRejected, "empty_story"};
        client_notes[note_id] = receipt;
        outcome.receipt = receipt;
        return outcome;
    }

    TimePoint anchor_time = event_time > received_time ? received_time : event_time;

    auto story_it = stories_.find(key);
    if (story_it == stories_.end()) {
        story_it = stories_.emplace(key, Story(key)).first;
    }
    Story& story = story_it->second;

    if (!story.IsPublished() && !story.HasContributionFrom(client_id)) {
        story.AddContribution(client_id, client->trust, anchor_time, received_time);

        double total_trust = story.CurrentTrust(received_time, config_.half_life);
        if (total_trust >= config_.threshold) {
            story.Publish(received_time);
            outcome.story_published = true;

            const std::vector<Submitter>& submitters = story.Submitters();
            for (size_t i = 0; i < submitters.size(); ++i) {
                int rank = static_cast<int>(i) + 1;
                stats_.RecordPublished(submitters[i].client_id, rank);

                if (rank <= config_.congratulated_k) {
                    CongratsMessage congrats;
                    congrats.client_id = submitters[i].client_id;
                    congrats.story = key;
                    congrats.rank = rank;
                    congrats.text = "Поздравляем, ваша заметка попала в печать. Вы были " +
                                     std::to_string(rank) + "-м, кто сказал нам об этой новости.";
                    outcome.congrats.push_back(std::move(congrats));
                }
            }
        }
    }

    stats_.RecordAccepted(client_id, key);

    Receipt receipt{note_id, ReceiptStatus::kAccepted, ""};
    client_notes[note_id] = receipt;
    outcome.receipt = receipt;
    return outcome;
}

ClientStats Engine::GetStats(const std::string& client_id) const { return stats_.Get(client_id); }

std::vector<ClientStats> Engine::GetAllStats() const { return stats_.GetAll(); }

}  // namespace newsdesk
