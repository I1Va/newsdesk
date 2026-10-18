#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/clock.h"
#include "core/engine.h"
#include "core/outbox.h"
#include "core/registry.h"
#include "platform/platform.h"
#include "protocol/message.h"
#include "storage/journal.h"

namespace newsdesk::server {

struct ServerConfig {
    std::string bind_address = "0.0.0.0";
    uint16_t port = 7000;
    std::string journal_path = "newsdesk.wal";
    double threshold = 0.75;
    int congratulated_k = 3;
    Duration half_life = std::chrono::hours(6);
    size_t outbox_max_size = 1000;
    std::vector<ClientInfo> clients;
};

struct Connection {
    platform::SocketHandle socket = platform::kInvalidSocket;
    std::string read_buffer;
    std::string write_buffer;
    bool authenticated = false;
    std::string client_id;
    Role role = Role::kReporter;
};

class Server {
public:
    Server(platform::Platform& platform, Clock& clock, ServerConfig config);

    void Run();

private:
    void AcceptConnections();
    void HandleReadable(Connection& connection);
    void HandleWritable(Connection& connection);
    void ProcessLine(Connection& connection, const std::string& line);
    void HandleHello(Connection& connection, const protocol::HelloMessage& hello);
    void HandleNote(Connection& connection, const protocol::NoteMessage& note);
    void HandleAck(Connection& connection, const protocol::AckMessage& ack);
    void HandleStats(Connection& connection, const protocol::StatsRequestMessage& request);
    void HandleBye(Connection& connection);
    void CloseConnection(Connection& connection);

    void SendDirect(Connection& connection, std::string line);
    void UpdatePollInterest(Connection& connection);
    void ReplayJournal();

    platform::Platform& platform_;
    Clock& clock_;
    ServerConfig config_;
    Registry registry_;
    Engine engine_;
    storage::Journal journal_;

    platform::SocketHandle listener_ = platform::kInvalidSocket;
    std::unordered_map<platform::SocketHandle, Connection> connections_;
    std::unordered_map<std::string, Outbox> outboxes_;
    std::unordered_map<std::string, platform::SocketHandle> connected_clients_;
};

}  // namespace newsdesk::server
