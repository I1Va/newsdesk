#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace newsdesk {

struct OutboxEntry {
    uint64_t seq;
    std::string payload;
};

class Outbox {
public:
    explicit Outbox(size_t max_size);

    uint64_t Enqueue(std::string payload);
    uint64_t NextSeq() const;

    void Ack(uint64_t seq);

    std::vector<OutboxEntry> MessagesAfter(uint64_t last_seq) const;

    size_t Size() const;
    bool IsFull() const;

private:
    size_t max_size_;
    uint64_t next_seq_ = 1;
    std::deque<OutboxEntry> entries_;
};

}  // namespace newsdesk
