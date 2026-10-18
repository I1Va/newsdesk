#pragma once

#include <cstdint>
#include <string>

#include "platform/platform.h"

namespace newsdesk::client {

struct ClientConfig {
    std::string server_address = "127.0.0.1";
    uint16_t server_port = 7000;
    std::string client_id;
};

class Client {
public:
    Client(platform::Platform& platform, ClientConfig config);

    // Connects, says hello, then alternates between reading stdin commands
    // and draining incoming server messages until the user quits or stdin closes.
    void Run();

private:
    bool WaitForConnect();
    void SendLine(const std::string& line);
    void PumpIncoming();
    void HandleStdinCommand(const std::string& command);

    platform::Platform& platform_;
    ClientConfig config_;
    platform::SocketHandle socket_ = platform::kInvalidSocket;
    std::string read_buffer_;
    uint64_t last_seq_ = 0;
};

}  // namespace newsdesk::client
