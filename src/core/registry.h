#pragma once

#include <string>
#include <unordered_map>

namespace newsdesk {

enum class Role {
    kReporter,
    kEditor,
};

struct ClientInfo {
    std::string id;
    double trust;
    Role role;
};

class Registry {
public:
    void Add(ClientInfo info);
    const ClientInfo* Find(const std::string& client_id) const;
    bool Exists(const std::string& client_id) const;

private:
    std::unordered_map<std::string, ClientInfo> clients_;
};

}  // namespace newsdesk
