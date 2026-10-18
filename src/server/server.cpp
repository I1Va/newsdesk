#include "server/server.h"

#include <iostream>

namespace newsdesk::server {

namespace {

constexpr int kPollTimeoutMs = 200;
constexpr size_t kMaxLineLength = 64 * 1024;

EngineConfig MakeEngineConfig(const ServerConfig& config) {
    return EngineConfig{config.threshold, config.congratulated_k, config.half_life};
}

}  // namespace

Server::Server(platform::Platform& platform, Clock& clock, ServerConfig config)
    : platform_(platform),
      clock_(clock),
      config_(config),
      engine_(registry_, MakeEngineConfig(config)),
      journal_(platform, config.journal_path) {
    for (const ClientInfo& client : config_.clients) {
        registry_.Add(client);
        outboxes_.emplace(client.id, Outbox(config_.outbox_max_size));
    }
    ReplayJournal();
}

void Server::ReplayJournal() {
    storage::ReplayResult replay = journal_.ReplayAll();
    if (replay.last_line_dropped) {
        std::cerr << "warning: journal had an incomplete trailing record, it was dropped\n";
    }
    for (const storage::JournalRecord& record : replay.records) {
        if (record.type == storage::RecordType::kNote) {
            const storage::NoteRecord& note = *record.note;
            engine_.HandleNote(note.client_id, note.note_id, note.rubric, note.topic,
                               note.event_time, note.received_time);
        } else if (record.type == storage::RecordType::kEnqueue) {
            const storage::EnqueueRecord& enqueue = *record.enqueue;
            auto it = outboxes_.find(enqueue.client_id);
            if (it != outboxes_.end()) {
                it->second.Enqueue(enqueue.payload);
            }
        } else if (record.type == storage::RecordType::kAck) {
            const storage::AckRecord& ack = *record.ack;
            auto it = outboxes_.find(ack.client_id);
            if (it != outboxes_.end()) {
                it->second.Ack(ack.seq);
            }
        }
    }
}

void Server::Run() {
    listener_ = platform_.Listen(config_.bind_address, config_.port);
    platform_.SetPollInterest(listener_, platform::PollInterest::kRead);

    while (!platform_.ShutdownRequested()) {
        std::vector<platform::PollEvent> events = platform_.Poll(kPollTimeoutMs);
        for (const platform::PollEvent& event : events) {
            if (event.socket == listener_) {
                if (event.readable) {
                    AcceptConnections();
                }
                continue;
            }

            auto it = connections_.find(event.socket);
            if (it == connections_.end()) {
                continue;
            }
            Connection& connection = it->second;

            if (event.error) {
                CloseConnection(connection);
                continue;
            }
            if (event.readable) {
                HandleReadable(connection);
            }
            if (connections_.count(event.socket) && event.writable) {
                HandleWritable(connection);
            }
        }
    }
}

void Server::AcceptConnections() {
    while (true) {
        platform::SocketHandle socket = platform_.Accept(listener_);
        if (socket == platform::kInvalidSocket) {
            break;
        }
        Connection connection;
        connection.socket = socket;
        connections_[socket] = connection;
        platform_.SetPollInterest(socket, platform::PollInterest::kRead);
    }
}

void Server::HandleReadable(Connection& connection) {
    char buffer[4096];
    while (true) {
        platform::IoResult result = platform_.Read(connection.socket, buffer, sizeof(buffer));
        if (result.status == platform::IoStatus::kOk) {
            connection.read_buffer.append(buffer, result.bytes);
            if (connection.read_buffer.size() > kMaxLineLength) {
                CloseConnection(connection);
                return;
            }
            continue;
        }
        if (result.status == platform::IoStatus::kWouldBlock) {
            break;
        }
        CloseConnection(connection);
        return;
    }

    size_t pos = 0;
    while (true) {
        size_t newline = connection.read_buffer.find('\n', pos);
        if (newline == std::string::npos) {
            break;
        }
        std::string line = connection.read_buffer.substr(pos, newline - pos);
        pos = newline + 1;
        ProcessLine(connection, line);
        if (connections_.count(connection.socket) == 0) {
            return;
        }
    }
    connection.read_buffer.erase(0, pos);
}

void Server::HandleWritable(Connection& connection) {
    if (connection.write_buffer.empty()) {
        return;
    }
    platform::IoResult result = platform_.Write(connection.socket, connection.write_buffer.data(),
                                                connection.write_buffer.size());
    if (result.status == platform::IoStatus::kOk) {
        connection.write_buffer.erase(0, result.bytes);
    } else if (result.status == platform::IoStatus::kError ||
               result.status == platform::IoStatus::kClosed) {
        CloseConnection(connection);
        return;
    }
    UpdatePollInterest(connection);
}

void Server::ProcessLine(Connection& connection, const std::string& line) {
    std::optional<protocol::ParsedMessage> parsed = protocol::ParseIncoming(line);
    if (!parsed.has_value()) {
        SendDirect(connection,
                   protocol::Serialize(protocol::ErrorMessage{"bad_format", "malformed message"}));
        return;
    }

    if (parsed->type == protocol::IncomingType::kHello) {
        HandleHello(connection, *parsed->hello);
        return;
    }
    if (parsed->type == protocol::IncomingType::kBye) {
        HandleBye(connection);
        return;
    }

    if (!connection.authenticated) {
        if (parsed->type == protocol::IncomingType::kNote) {
            Receipt receipt{parsed->note->note_id, ReceiptStatus::kRejected, "not_authenticated"};
            SendDirect(connection, protocol::Serialize(receipt));
        } else {
            SendDirect(connection, protocol::Serialize(protocol::ErrorMessage{
                                       "not_authenticated", "handshake required"}));
        }
        return;
    }

    if (parsed->type == protocol::IncomingType::kNote) {
        HandleNote(connection, *parsed->note);
    } else if (parsed->type == protocol::IncomingType::kAck) {
        HandleAck(connection, *parsed->ack);
    } else if (parsed->type == protocol::IncomingType::kStats) {
        HandleStats(connection, *parsed->stats);
    }
}

void Server::HandleHello(Connection& connection, const protocol::HelloMessage& hello) {
    const ClientInfo* client = registry_.Find(hello.client_id);
    if (client == nullptr) {
        SendDirect(connection,
                   protocol::Serialize(protocol::ErrorMessage{"unknown_client", "no such client"}));
        CloseConnection(connection);
        return;
    }

    connection.authenticated = true;
    connection.client_id = hello.client_id;
    connection.role = client->role;
    connected_clients_[hello.client_id] = connection.socket;

    SendDirect(connection, protocol::Serialize(protocol::WelcomeMessage{client->id, client->role}));

    auto outbox_it = outboxes_.find(hello.client_id);
    if (outbox_it != outboxes_.end()) {
        for (const OutboxEntry& entry : outbox_it->second.MessagesAfter(hello.last_seq)) {
            SendDirect(connection, entry.payload);
        }
    }
}

void Server::HandleNote(Connection& connection, const protocol::NoteMessage& note) {
    TimePoint received_time = clock_.Now();
    NoteOutcome outcome = engine_.HandleNote(connection.client_id, note.note_id, note.rubric,
                                             note.topic, note.event_time, received_time);

    storage::NoteRecord note_record;
    note_record.client_id = connection.client_id;
    note_record.note_id = note.note_id;
    note_record.rubric = note.rubric;
    note_record.topic = note.topic;
    note_record.event_time = note.event_time;
    note_record.received_time = received_time;
    note_record.receipt = outcome.receipt;

    storage::JournalRecord record;
    record.record_seq = 0;
    record.type = storage::RecordType::kNote;
    record.note = note_record;
    journal_.Append(record);

    SendDirect(connection, protocol::Serialize(outcome.receipt));

    for (const CongratsMessage& congrats : outcome.congrats) {
        auto outbox_it = outboxes_.find(congrats.client_id);
        if (outbox_it == outboxes_.end()) {
            continue;
        }
        uint64_t seq = outbox_it->second.NextSeq();
        protocol::CongratsPayload payload{seq, congrats.story, congrats.rank, congrats.text};
        std::string line = protocol::Serialize(payload);
        outbox_it->second.Enqueue(line);

        storage::JournalRecord enqueue_record;
        enqueue_record.record_seq = 0;
        enqueue_record.type = storage::RecordType::kEnqueue;
        enqueue_record.enqueue = storage::EnqueueRecord{congrats.client_id, seq, line};
        journal_.Append(enqueue_record);

        auto conn_it = connected_clients_.find(congrats.client_id);
        if (conn_it != connected_clients_.end()) {
            auto connection_it = connections_.find(conn_it->second);
            if (connection_it != connections_.end()) {
                connection_it->second.write_buffer += line;
                connection_it->second.write_buffer += "\n";
                UpdatePollInterest(connection_it->second);
                HandleWritable(connection_it->second);
            }
        }
    }
}

void Server::HandleAck(Connection& connection, const protocol::AckMessage& ack) {
    auto it = outboxes_.find(connection.client_id);
    if (it != outboxes_.end()) {
        it->second.Ack(ack.seq);
    }

    storage::JournalRecord record;
    record.record_seq = 0;
    record.type = storage::RecordType::kAck;
    record.ack = storage::AckRecord{connection.client_id, ack.seq};
    journal_.Append(record);
}

void Server::HandleStats(Connection& connection, const protocol::StatsRequestMessage& request) {
    if (request.target_client_id.has_value() && *request.target_client_id != connection.client_id) {
        if (connection.role != Role::kEditor) {
            SendDirect(connection,
                       protocol::Serialize(protocol::ErrorMessage{"forbidden", "editor only"}));
            return;
        }
        protocol::StatsResponseMessage response;
        response.stats.push_back(engine_.GetStats(*request.target_client_id));
        SendDirect(connection, protocol::Serialize(response));
        return;
    }

    if (!request.target_client_id.has_value() && connection.role == Role::kEditor) {
        protocol::StatsResponseMessage response;
        response.stats = engine_.GetAllStats();
        SendDirect(connection, protocol::Serialize(response));
        return;
    }

    protocol::StatsResponseMessage response;
    response.stats.push_back(engine_.GetStats(connection.client_id));
    SendDirect(connection, protocol::Serialize(response));
}

void Server::HandleBye(Connection& connection) { CloseConnection(connection); }

void Server::SendDirect(Connection& connection, std::string line) {
    connection.write_buffer += line;
    connection.write_buffer += "\n";
    UpdatePollInterest(connection);
    HandleWritable(connection);
}

void Server::UpdatePollInterest(Connection& connection) {
    platform::PollInterest interest = connection.write_buffer.empty()
                                          ? platform::PollInterest::kRead
                                          : platform::PollInterest::kReadWrite;
    platform_.SetPollInterest(connection.socket, interest);
}

void Server::CloseConnection(Connection& connection) {
    platform_.RemoveFromPoll(connection.socket);
    platform_.CloseSocket(connection.socket);
    if (!connection.client_id.empty()) {
        connected_clients_.erase(connection.client_id);
    }
    connections_.erase(connection.socket);
}

}  // namespace newsdesk::server
