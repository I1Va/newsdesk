#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/clock.h"
#include "core/engine.h"
#include "core/story.h"
#include "platform/platform.h"

namespace newsdesk::storage {

enum class RecordType {
    kNote,
    kPublish,
    kEnqueue,
    kAck,
};

struct NoteRecord {
    std::string client_id;
    std::string note_id;
    std::string rubric;
    std::string topic;
    TimePoint event_time;
    TimePoint received_time;
    Receipt receipt;
};

struct PublishRecord {
    StoryKey story;
    TimePoint published_at;
    std::vector<Submitter> submitters;
};

struct EnqueueRecord {
    std::string client_id;
    uint64_t seq;
    std::string payload;  // serialized message body
};

struct AckRecord {
    std::string client_id;
    uint64_t seq;
};

struct JournalRecord {
    uint64_t record_seq;
    RecordType type;
    std::optional<NoteRecord> note;
    std::optional<PublishRecord> publish;
    std::optional<EnqueueRecord> enqueue;
    std::optional<AckRecord> ack;
};

// кодирует запись в виде одной строки
std::string EncodeRecord(const JournalRecord& record);

enum class DecodeStatus {
    kOk,
    kCorrupt,  // при несовпадении checksum или при ошибки парсинга JSON
    kTruncated,
};

struct DecodeResult {
    DecodeStatus status;
    JournalRecord record;  // валидно только при status == kOk
};

DecodeResult DecodeRecord(const std::string& line);

struct ReplayResult {
    std::vector<JournalRecord> records;
    bool last_line_dropped = false;
};

class Journal {
public:
    Journal(platform::Platform& platform, std::string path);

    // Добавляет один record и делает fsyncs
    // Отправка считается успешной только после явного сброса данных на диск.
    void Append(const JournalRecord& record);

    // загружает журнал из диска в оперативку.
    ReplayResult ReplayAll();

private:
    platform::Platform& platform_;
    std::string path_;
    platform::FileHandle file_;
    uint64_t next_record_seq_ = 1;
};

}  // namespace newsdesk::storage
