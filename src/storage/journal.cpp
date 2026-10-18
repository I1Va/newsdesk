#include "storage/journal.h"

#include <nlohmann/json.hpp>
#include <stdexcept>

namespace newsdesk::storage {

namespace {

int64_t ToMillis(TimePoint time) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count();
}

TimePoint FromMillis(int64_t millis) { return TimePoint{} + std::chrono::milliseconds(millis); }

std::string TypeToString(RecordType type) {
    switch (type) {
        case RecordType::kNote:
            return "note";
        case RecordType::kPublish:
            return "publish";
        case RecordType::kEnqueue:
            return "enqueue";
        case RecordType::kAck:
            return "ack";
    }
    return "note";
}

std::string StatusToString(ReceiptStatus status) {
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

ReceiptStatus StatusFromString(const std::string& status) {
    if (status == "accepted") return ReceiptStatus::kAccepted;
    if (status == "duplicate") return ReceiptStatus::kDuplicate;
    return ReceiptStatus::kRejected;
}

nlohmann::json BodyToJson(const JournalRecord& record) {
    nlohmann::json body;
    switch (record.type) {
        case RecordType::kNote: {
            const NoteRecord& note = *record.note;
            body["client_id"] = note.client_id;
            body["note_id"] = note.note_id;
            body["rubric"] = note.rubric;
            body["topic"] = note.topic;
            body["event_time"] = ToMillis(note.event_time);
            body["received_time"] = ToMillis(note.received_time);
            body["status"] = StatusToString(note.receipt.status);
            body["reason"] = note.receipt.reason;
            break;
        }
        case RecordType::kPublish: {
            const PublishRecord& publish = *record.publish;
            body["rubric"] = publish.story.rubric;
            body["topic"] = publish.story.topic;
            body["published_at"] = ToMillis(publish.published_at);
            nlohmann::json submitters = nlohmann::json::array();
            for (const Submitter& submitter : publish.submitters) {
                submitters.push_back({
                    {"client_id", submitter.client_id},
                    {"received_time", ToMillis(submitter.received_time)},
                });
            }
            body["submitters"] = submitters;
            break;
        }
        case RecordType::kEnqueue: {
            const EnqueueRecord& enqueue = *record.enqueue;
            body["client_id"] = enqueue.client_id;
            body["seq"] = enqueue.seq;
            body["payload"] = enqueue.payload;
            break;
        }
        case RecordType::kAck: {
            const AckRecord& ack = *record.ack;
            body["client_id"] = ack.client_id;
            body["seq"] = ack.seq;
            break;
        }
    }
    return body;
}

uint32_t Checksum(const std::string& data) {
    uint32_t hash = 2166136261u;
    for (unsigned char c : data) {
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

}  // namespace

std::string EncodeRecord(const JournalRecord& record) {
    nlohmann::json envelope;
    envelope["seq"] = record.record_seq;
    envelope["type"] = TypeToString(record.type);
    envelope["body"] = BodyToJson(record);

    std::string body_text = envelope.dump();
    uint32_t checksum = Checksum(body_text);

    nlohmann::json line;
    line["seq"] = record.record_seq;
    line["type"] = TypeToString(record.type);
    line["body"] = envelope["body"];
    line["checksum"] = checksum;
    return line.dump();
}

DecodeResult DecodeRecord(const std::string& line) {
    DecodeResult result;

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(line);
    } catch (const nlohmann::json::exception&) {
        result.status = DecodeStatus::kTruncated;
        return result;
    }

    if (!j.is_object() || !j.contains("seq") || !j.contains("type") || !j.contains("body") ||
        !j.contains("checksum")) {
        result.status = DecodeStatus::kCorrupt;
        return result;
    }

    nlohmann::json envelope;
    envelope["seq"] = j["seq"];
    envelope["type"] = j["type"];
    envelope["body"] = j["body"];
    uint32_t expected_checksum = Checksum(envelope.dump());
    if (expected_checksum != j["checksum"].get<uint32_t>()) {
        result.status = DecodeStatus::kCorrupt;
        return result;
    }

    try {
        JournalRecord record;
        record.record_seq = j["seq"].get<uint64_t>();
        std::string type = j["type"].get<std::string>();
        const nlohmann::json& body = j["body"];

        if (type == "note") {
            record.type = RecordType::kNote;
            NoteRecord note;
            note.client_id = body.at("client_id").get<std::string>();
            note.note_id = body.at("note_id").get<std::string>();
            note.rubric = body.at("rubric").get<std::string>();
            note.topic = body.at("topic").get<std::string>();
            note.event_time = FromMillis(body.at("event_time").get<int64_t>());
            note.received_time = FromMillis(body.at("received_time").get<int64_t>());
            note.receipt.note_id = note.note_id;
            note.receipt.status = StatusFromString(body.at("status").get<std::string>());
            note.receipt.reason = body.at("reason").get<std::string>();
            record.note = note;
        } else if (type == "publish") {
            record.type = RecordType::kPublish;
            PublishRecord publish;
            publish.story.rubric = body.at("rubric").get<std::string>();
            publish.story.topic = body.at("topic").get<std::string>();
            publish.published_at = FromMillis(body.at("published_at").get<int64_t>());
            for (const auto& entry : body.at("submitters")) {
                Submitter submitter;
                submitter.client_id = entry.at("client_id").get<std::string>();
                submitter.received_time = FromMillis(entry.at("received_time").get<int64_t>());
                publish.submitters.push_back(submitter);
            }
            record.publish = publish;
        } else if (type == "enqueue") {
            record.type = RecordType::kEnqueue;
            EnqueueRecord enqueue;
            enqueue.client_id = body.at("client_id").get<std::string>();
            enqueue.seq = body.at("seq").get<uint64_t>();
            enqueue.payload = body.at("payload").get<std::string>();
            record.enqueue = enqueue;
        } else if (type == "ack") {
            record.type = RecordType::kAck;
            AckRecord ack;
            ack.client_id = body.at("client_id").get<std::string>();
            ack.seq = body.at("seq").get<uint64_t>();
            record.ack = ack;
        } else {
            result.status = DecodeStatus::kCorrupt;
            return result;
        }

        result.status = DecodeStatus::kOk;
        result.record = record;
        return result;
    } catch (const nlohmann::json::exception&) {
        result.status = DecodeStatus::kCorrupt;
        return result;
    }
}

Journal::Journal(platform::Platform& platform, std::string path)
    : platform_(platform), path_(std::move(path)), file_(platform_.OpenAppend(path_)) {}

void Journal::Append(const JournalRecord& record) {
    std::string line = EncodeRecord(record) + "\n";
    platform_.Append(file_, line.data(), line.size());
    platform_.Sync(file_);
    next_record_seq_ = record.record_seq + 1;
}

ReplayResult Journal::ReplayAll() const {
    ReplayResult result;
    std::string content = platform_.ReadAll(path_);

    size_t pos = 0;
    while (pos < content.size()) {
        size_t newline = content.find('\n', pos);
        bool is_last_line = newline == std::string::npos;
        std::string line = is_last_line ? content.substr(pos) : content.substr(pos, newline - pos);
        pos = is_last_line ? content.size() : newline + 1;

        if (line.empty()) {
            continue;
        }

        DecodeResult decoded = DecodeRecord(line);
        if (decoded.status == DecodeStatus::kOk) {
            result.records.push_back(decoded.record);
            continue;
        }

        if (is_last_line) {
            result.last_line_dropped = true;
            break;
        }

        throw std::runtime_error("journal corrupted before the last line");
    }

    return result;
}

}  // namespace newsdesk::storage
