#include <chrono>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

#include "client/client.h"
#include "core/clock.h"
#include "platform/platform.h"
#include "server/server.h"

namespace {

std::string ReadFile(const std::string& path) {
    std::ifstream in(path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::string GetArg(int argc, char** argv, const std::string& name, const std::string& fallback) {
    for (int i = 0; i < argc - 1; ++i) {
        if (std::string(argv[i]) == name) {
            return argv[i + 1];
        }
    }
    return fallback;
}

void SplitHostPort(const std::string& listen, std::string* host, uint16_t* port) {
    size_t colon = listen.rfind(':');
    *host = listen.substr(0, colon);
    *port = static_cast<uint16_t>(std::stoi(listen.substr(colon + 1)));
}

int RunServer(int argc, char** argv) {
    std::string config_path = GetArg(argc, argv, "--config", "config/example.json");
    nlohmann::json config_json = nlohmann::json::parse(ReadFile(config_path));

    newsdesk::server::ServerConfig config;
    config.threshold = config_json.value("threshold", 0.75);
    config.congratulated_k = config_json.value("congratulated_k", 3);
    config.half_life = std::chrono::hours(config_json.value("half_life_hours", 6));
    config.journal_path = config_json.value("journal_path", "newsdesk.wal");
    SplitHostPort(config_json.value("listen", "0.0.0.0:7000"), &config.bind_address, &config.port);

    for (const auto& entry : config_json.value("clients", nlohmann::json::array())) {
        newsdesk::ClientInfo client;
        client.id = entry.at("id").get<std::string>();
        client.trust = entry.at("trust").get<double>();
        std::string role = entry.value("role", "reporter");
        client.role = role == "editor" ? newsdesk::Role::kEditor : newsdesk::Role::kReporter;
        config.clients.push_back(client);
    }

    std::string listen_override = GetArg(argc, argv, "--listen", "");
    if (!listen_override.empty()) {
        SplitHostPort(listen_override, &config.bind_address, &config.port);
    }

    auto platform = newsdesk::platform::CreatePlatform();
    newsdesk::SystemClock clock;
    newsdesk::server::Server server(*platform, clock, config);

    std::cout << "newsdesk server listening on " << config.bind_address << ":" << config.port
               << "\n";
    server.Run();
    return 0;
}

int RunClient(int argc, char** argv) {
    newsdesk::client::ClientConfig config;
    config.server_address = GetArg(argc, argv, "--server", "127.0.0.1");
    config.server_port = static_cast<uint16_t>(std::stoi(GetArg(argc, argv, "--port", "7000")));
    config.client_id = GetArg(argc, argv, "--client-id", "");

    if (config.client_id.empty()) {
        std::cerr << "usage: newsdesk client --client-id ID [--server HOST] [--port PORT]\n";
        return 1;
    }

    auto platform = newsdesk::platform::CreatePlatform();
    newsdesk::client::Client client(*platform, config);
    client.Run();
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: newsdesk <server|client> [options]\n";
        return 1;
    }

    std::string mode = argv[1];
    if (mode == "server") {
        return RunServer(argc, argv);
    }
    if (mode == "client") {
        return RunClient(argc, argv);
    }

    std::cerr << "unknown mode: " << mode << "\n";
    return 1;
}
