#include "core/outbox.h"

namespace newsdesk {

Outbox::Outbox(size_t max_size) : max_size_(max_size) {}

uint64_t Outbox::Enqueue(std::string payload) {
    uint64_t seq = next_seq_++;
    entries_.push_back(OutboxEntry{seq, std::move(payload)});
    return seq;
}

uint64_t Outbox::NextSeq() const { return next_seq_; }

void Outbox::Ack(uint64_t seq) {
    while (!entries_.empty() && entries_.front().seq <= seq) {
        entries_.pop_front();
    }
}

std::vector<OutboxEntry> Outbox::MessagesAfter(uint64_t last_seq) const {
    std::vector<OutboxEntry> result;
    for (const auto& entry : entries_) {
        if (entry.seq > last_seq) {
            result.push_back(entry);
        }
    }
    return result;
}

size_t Outbox::Size() const { return entries_.size(); }

bool Outbox::IsFull() const { return entries_.size() >= max_size_; }

}  // namespace newsdesk
