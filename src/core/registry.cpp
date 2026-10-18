#include "core/registry.h"

namespace newsdesk {

void Registry::Add(ClientInfo info) { clients_[info.id] = std::move(info); }

const ClientInfo* Registry::Find(const std::string& client_id) const {
    auto it = clients_.find(client_id);
    return it == clients_.end() ? nullptr : &it->second;
}

bool Registry::Exists(const std::string& client_id) const {
    return clients_.find(client_id) != clients_.end();
}

}  // namespace newsdesk
