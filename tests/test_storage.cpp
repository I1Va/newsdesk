#include <doctest/doctest.h>

#include <stdexcept>
#include <unordered_map>

#include "storage/journal.h"

using namespace newsdesk;
using namespace newsdesk::storage;
using namespace std::chrono_literals;

namespace {

class FakePlatform : public platform::Platform {
public:
    platform::SocketHandle Listen(const std::string&, uint16_t) override {
        return platform::kInvalidSocket;
    }
    platform::SocketHandle Connect(const std::string&, uint16_t) override {
        return platform::kInvalidSocket;
    }
    platform::SocketHandle Accept(platform::SocketHandle) override {
        return platform::kInvalidSocket;
    }
    bool ConnectSucceeded(platform::SocketHandle) override { return false; }
    platform::IoResult Read(platform::SocketHandle, char*, size_t) override {
        return platform::IoResult{platform::IoStatus::kError, 0};
    }
    platform::IoResult Write(platform::SocketHandle, const char*, size_t) override {
        return platform::IoResult{platform::IoStatus::kError, 0};
    }
    void CloseSocket(platform::SocketHandle) override {}
    void SetPollInterest(platform::SocketHandle, platform::PollInterest) override {}
    void RemoveFromPoll(platform::SocketHandle) override {}
    std::vector<platform::PollEvent> Poll(int) override { return {}; }
    bool ShutdownRequested() override { return false; }

    platform::FileHandle OpenAppend(const std::string& path) override {
        files_[path];
        platform::FileHandle handle = next_handle_++;
        handle_paths_[handle] = path;
        return handle;
    }

    platform::IoResult Append(platform::FileHandle file, const char* data, size_t size) override {
        files_[handle_paths_.at(file)].append(data, size);
        return platform::IoResult{platform::IoStatus::kOk, size};
    }

    void Sync(platform::FileHandle) override {}
    void CloseFile(platform::FileHandle) override {}

    std::string ReadAll(const std::string& path) override {
        auto it = files_.find(path);
        return it == files_.end() ? "" : it->second;
    }

    // Test-only helpers to inject corruption directly into the backing "file".
    void AppendRaw(const std::string& path, const std::string& data) { files_[path] += data; }
    void SetRaw(const std::string& path, const std::string& content) { files_[path] = content; }

private:
    std::unordered_map<std::string, std::string> files_;
    std::unordered_map<platform::FileHandle, std::string> handle_paths_;
    platform::FileHandle next_handle_ = 1;
};

}  // namespace

TEST_CASE("EncodeRecord/DecodeRecord round-trips a note record") {
    JournalRecord record;
    record.record_seq = 1;
    record.type = RecordType::kNote;
    NoteRecord note;
    note.client_id = "alice";
    note.note_id = "n1";
    note.rubric = "local";
    note.topic = "fire";
    note.event_time = TimePoint{} + 1000h;
    note.received_time = TimePoint{} + 1001h;
    note.receipt = Receipt{"n1", ReceiptStatus::kAccepted, ""};
    record.note = note;

    DecodeResult decoded = DecodeRecord(EncodeRecord(record));
    REQUIRE(decoded.status == DecodeStatus::kOk);
    CHECK(decoded.record.type == RecordType::kNote);
    REQUIRE(decoded.record.note.has_value());
    CHECK(decoded.record.note->client_id == "alice");
    CHECK(decoded.record.note->note_id == "n1");
    CHECK(decoded.record.note->event_time == note.event_time);
    CHECK(decoded.record.note->received_time == note.received_time);
    CHECK(decoded.record.note->receipt.status == ReceiptStatus::kAccepted);
}

TEST_CASE("EncodeRecord/DecodeRecord round-trips a publish record with full submitter order") {
    JournalRecord record;
    record.record_seq = 2;
    record.type = RecordType::kPublish;
    PublishRecord publish;
    publish.story = StoryKey{"local", "fire"};
    publish.published_at = TimePoint{} + 1000h;
    publish.submitters = {Submitter{"alice", TimePoint{} + 999h}, Submitter{"bob", TimePoint{} + 1000h}};
    record.publish = publish;

    DecodeResult decoded = DecodeRecord(EncodeRecord(record));
    REQUIRE(decoded.status == DecodeStatus::kOk);
    REQUIRE(decoded.record.publish.has_value());
    CHECK(decoded.record.publish->story.rubric == "local");
    CHECK(decoded.record.publish->story.topic == "fire");
    REQUIRE(decoded.record.publish->submitters.size() == 2);
    CHECK(decoded.record.publish->submitters[0].client_id == "alice");
    CHECK(decoded.record.publish->submitters[1].client_id == "bob");
}

TEST_CASE("EncodeRecord/DecodeRecord round-trips an enqueue record") {
    JournalRecord record;
    record.record_seq = 3;
    record.type = RecordType::kEnqueue;
    record.enqueue = EnqueueRecord{"alice", 7, R"({"type":"congrats"})"};

    DecodeResult decoded = DecodeRecord(EncodeRecord(record));
    REQUIRE(decoded.status == DecodeStatus::kOk);
    REQUIRE(decoded.record.enqueue.has_value());
    CHECK(decoded.record.enqueue->client_id == "alice");
    CHECK(decoded.record.enqueue->seq == 7);
    CHECK(decoded.record.enqueue->payload == R"({"type":"congrats"})");
}

TEST_CASE("EncodeRecord/DecodeRecord round-trips an ack record") {
    JournalRecord record;
    record.record_seq = 4;
    record.type = RecordType::kAck;
    record.ack = AckRecord{"bob", 9};

    DecodeResult decoded = DecodeRecord(EncodeRecord(record));
    REQUIRE(decoded.status == DecodeStatus::kOk);
    REQUIRE(decoded.record.ack.has_value());
    CHECK(decoded.record.ack->client_id == "bob");
    CHECK(decoded.record.ack->seq == 9);
}

TEST_CASE("DecodeRecord detects a checksum mismatch") {
    JournalRecord record;
    record.record_seq = 1;
    record.type = RecordType::kAck;
    record.ack = AckRecord{"bob", 1};
    std::string line = EncodeRecord(record);

    size_t pos = line.find("bob");
    REQUIRE(pos != std::string::npos);
    line[pos] = 'x';

    DecodeResult decoded = DecodeRecord(line);
    CHECK(decoded.status == DecodeStatus::kCorrupt);
}

TEST_CASE("DecodeRecord reports a cut-off line as truncated, not corrupt") {
    JournalRecord record;
    record.record_seq = 1;
    record.type = RecordType::kAck;
    record.ack = AckRecord{"bob", 1};
    std::string line = EncodeRecord(record);

    DecodeResult decoded = DecodeRecord(line.substr(0, line.size() / 2));
    CHECK(decoded.status == DecodeStatus::kTruncated);
}

TEST_CASE("DecodeRecord rejects valid JSON missing a required envelope field") {
    DecodeResult decoded = DecodeRecord(R"({"seq":1,"type":"ack","body":{"client_id":"bob","seq":1}})");
    CHECK(decoded.status == DecodeStatus::kCorrupt);
}

TEST_CASE("Journal Append numbers records and ReplayAll reads them back in order") {
    FakePlatform platform;
    Journal journal(platform, "test.wal");

    JournalRecord first;
    first.type = RecordType::kAck;
    first.ack = AckRecord{"alice", 1};
    journal.Append(first);

    JournalRecord second;
    second.type = RecordType::kAck;
    second.ack = AckRecord{"alice", 2};
    journal.Append(second);

    ReplayResult replay = journal.ReplayAll();
    REQUIRE(replay.records.size() == 2);
    CHECK(replay.records[0].record_seq == 1);
    CHECK(replay.records[1].record_seq == 2);
    CHECK_FALSE(replay.last_line_dropped);
}

TEST_CASE("Journal continues record numbering after a restart") {
    FakePlatform platform;
    {
        Journal journal(platform, "test.wal");
        JournalRecord record;
        record.type = RecordType::kAck;
        record.ack = AckRecord{"alice", 1};
        journal.Append(record);
    }

    Journal reopened(platform, "test.wal");
    reopened.ReplayAll();

    JournalRecord next;
    next.type = RecordType::kAck;
    next.ack = AckRecord{"alice", 2};
    reopened.Append(next);

    ReplayResult replay = reopened.ReplayAll();
    REQUIRE(replay.records.size() == 2);
    CHECK(replay.records[1].record_seq == 2);
}

TEST_CASE("ReplayAll drops a truncated trailing line and still starts up") {
    FakePlatform platform;
    {
        Journal journal(platform, "test.wal");
        JournalRecord record;
        record.type = RecordType::kAck;
        record.ack = AckRecord{"alice", 1};
        journal.Append(record);
    }
    platform.AppendRaw("test.wal", R"({"seq":2,"type":"ack","bo)");

    Journal reopened(platform, "test.wal");
    ReplayResult replay = reopened.ReplayAll();
    CHECK(replay.records.size() == 1);
    CHECK(replay.last_line_dropped);
}

TEST_CASE("ReplayAll throws when a non-trailing line is corrupted") {
    FakePlatform platform;
    {
        Journal journal(platform, "test.wal");
        JournalRecord first;
        first.type = RecordType::kAck;
        first.ack = AckRecord{"alice", 1};
        journal.Append(first);

        JournalRecord second;
        second.type = RecordType::kAck;
        second.ack = AckRecord{"alice", 2};
        journal.Append(second);
    }

    std::string content = platform.ReadAll("test.wal");
    size_t pos = content.find("alice");
    REQUIRE(pos != std::string::npos);
    content[pos] = 'X';
    platform.SetRaw("test.wal", content);

    Journal reopened(platform, "test.wal");
    CHECK_THROWS_AS(reopened.ReplayAll(), std::runtime_error);
}
