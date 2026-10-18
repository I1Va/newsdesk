#include "client/client.h"

#include <chrono>
#include <iostream>
#include <sstream>

#include "protocol/message.h"

namespace newsdesk::client {

namespace {
constexpr platform::SocketHandle kStdin = 0;
}

Client::Client(platform::Platform& platform, ClientConfig config)
    : platform_(platform), config_(config) {}

void Client::SendLine(const std::string& line) {
    std::string data = line + "\n";
    size_t sent = 0;
    while (sent < data.size()) {
        platform::IoResult result =
            platform_.Write(socket_, data.data() + sent, data.size() - sent);
        if (result.status == platform::IoStatus::kOk) {
            sent += result.bytes;
        } else if (result.status == platform::IoStatus::kWouldBlock) {
            continue;
        } else {
            std::cerr << "connection lost\n";
            return;
        }
    }
}

void Client::PumpIncoming() {
    char buffer[4096];
    while (true) {
        platform::IoResult result = platform_.Read(socket_, buffer, sizeof(buffer));
        if (result.status == platform::IoStatus::kOk) {
            read_buffer_.append(buffer, result.bytes);
            continue;
        }
        if (result.status == platform::IoStatus::kWouldBlock) {
            break;
        }
        std::cerr << "server closed the connection\n";
        return;
    }

    size_t pos = 0;
    while (true) {
        size_t newline = read_buffer_.find('\n', pos);
        if (newline == std::string::npos) {
            break;
        }
        std::string line = read_buffer_.substr(pos, newline - pos);
        pos = newline + 1;
        std::cout << line << "\n";

        std::optional<protocol::ParsedServerMessage> parsed = protocol::ParseServerMessage(line);
        if (parsed.has_value() && parsed->type == protocol::OutgoingType::kCongrats) {
            uint64_t seq = parsed->congrats->seq;
            last_seq_ = seq;
            SendLine(protocol::Serialize(protocol::AckMessage{seq}));
        }
    }
    read_buffer_.erase(0, pos);
}

void Client::HandleStdinCommand(const std::string& command) {
    std::istringstream stream(command);
    std::string verb;
    stream >> verb;

    if (verb == "note") {
        std::string rest;
        std::getline(stream, rest);
        size_t sep = rest.find('|');
        if (sep == std::string::npos) {
            std::cout << "usage: note <rubric>|<topic>\n";
            return;
        }
        std::string rubric = rest.substr(1, sep - 1);
        std::string topic = rest.substr(sep + 1);
        protocol::NoteMessage note;
        note.note_id = std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                          std::chrono::system_clock::now().time_since_epoch())
                                          .count());
        note.rubric = rubric;
        note.topic = topic;
        note.event_time = std::chrono::system_clock::now();
        SendLine(protocol::Serialize(note));
    } else if (verb == "stats") {
        protocol::StatsRequestMessage request;
        std::string target;
        stream >> target;
        if (!target.empty()) {
            request.target_client_id = target;
        }
        SendLine(protocol::Serialize(request));
    } else if (verb == "bye") {
        SendLine(protocol::Serialize(protocol::ByeMessage{}));
    } else {
        std::cout << "commands: note <rubric>|<topic> ; stats [client_id] ; bye\n";
    }
}

bool Client::WaitForConnect() {
    platform_.SetPollInterest(socket_, platform::PollInterest::kReadWrite);
    for (int attempt = 0; attempt < 50; ++attempt) {
        std::vector<platform::PollEvent> events = platform_.Poll(200);
        for (const platform::PollEvent& event : events) {
            if (event.socket != socket_) {
                continue;
            }
            if (event.error) {
                return false;
            }
            if (event.writable || event.readable) {
                return platform_.ConnectSucceeded(socket_);
            }
        }
    }
    return false;
}

void Client::Run() {
    socket_ = platform_.Connect(config_.server_address, config_.server_port);
    if (socket_ == platform::kInvalidSocket) {
        std::cerr << "failed to connect\n";
        return;
    }
    if (!WaitForConnect()) {
        std::cerr << "failed to connect\n";
        platform_.CloseSocket(socket_);
        return;
    }

    platform_.SetPollInterest(socket_, platform::PollInterest::kRead);
    platform_.SetPollInterest(kStdin, platform::PollInterest::kRead);

    SendLine(protocol::Serialize(protocol::HelloMessage{config_.client_id, last_seq_}));
    std::cout << "connected as " << config_.client_id << "\n";

    while (true) {
        std::vector<platform::PollEvent> events = platform_.Poll(500);
        for (const platform::PollEvent& event : events) {
            if (event.socket == socket_) {
                if (event.error) {
                    std::cerr << "connection error\n";
                    return;
                }
                if (event.readable) {
                    PumpIncoming();
                }
            } else if (event.socket == kStdin && event.readable) {
                std::string command;
                if (!std::getline(std::cin, command)) {
                    SendLine(protocol::Serialize(protocol::ByeMessage{}));
                    return;
                }
                HandleStdinCommand(command);
            }
        }
    }
}

}  // namespace newsdesk::client
