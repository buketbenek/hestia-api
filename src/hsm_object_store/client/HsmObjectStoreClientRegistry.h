#pragma once

#include "HsmObjectStoreClient.h"
#include "HsmObjectStoreClientSpec.h"

#include <filesystem>
#include <set>
#include <string>
#include <unordered_map>

namespace hestia {

class ObjectStorePluginHandler {
  public:
    using Ptr = std::unique_ptr<ObjectStorePluginHandler>;

    ObjectStorePluginHandler(
        const std::vector<std::filesystem::directory_entry>& search_paths);

    bool has_plugin(const std::string& identifier);

    ObjectStoreClient::Ptr get_client(
        const HsmObjectStoreClientSpec& client_spec) const;

  private:
    std::vector<std::filesystem::directory_entry> m_search_paths;
};

class HsmObjectStoreClientRegistry {
  public:
    using Ptr = std::unique_ptr<HsmObjectStoreClientRegistry>;
    HsmObjectStoreClientRegistry(ObjectStorePluginHandler::Ptr plugin_handler);

    bool is_client_type_available(
        const HsmObjectStoreClientSpec& client_spec) const;

    ObjectStoreClient::Ptr get_client(
        const HsmObjectStoreClientSpec& client_spec) const;

  private:
    ObjectStorePluginHandler::Ptr m_plugin_handler;
};

using TierBackendRegistry =
    std::unordered_map<uint8_t, HsmObjectStoreClientSpec>;
}  // namespace hestia