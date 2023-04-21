#pragma once

#include "HsmObjectStoreClient.h"

#include <filesystem>
#include <set>
#include <string>
#include <unordered_map>

struct ObjectStoreClientType {
    enum class Type { HSM, BASIC };

    enum class Source { BUILT_IN, PLUGIN, MOCK };

    ObjectStoreClientType(
        Type client_type, Source source, const std::string& identifier) :
        m_type(client_type), m_source(source), mIdentifier(identifier)
    {
    }

    bool is_hsm() const { return m_type == Type::HSM; }

    Type m_type{Type::BASIC};
    Source m_source{Source::BUILT_IN};
    std::string m_identifier;
};

class ObjectStorePluginHandler {
  public:
    using Ptr = std::unique_ptr<ObjectStorePluginHandler>;

    ObjectStorePluginHandler(
        const std::vector<std::filesystem::directory_entry>& search_paths);

    bool has_plugin(const std::string& identifier);

    ostk::ObjectStoreClient::Ptr get_client(
        ObjectStoreClientType clientType) const;

  private:
    std::vector<std::filesystem::directory_entry> m_seach_paths;
};

class HsmObjectStoreClientRegistry {
  public:
    using Ptr = std::unique_ptr<HsmObjectStoreClientRegistry>;
    HsmObjectStoreClientRegistry(ObjectStorePluginHandler::Ptr plugin_handler);

    bool is_client_type_available(ObjectStoreClientType client_type) const;

    ostk::ObjectStoreClient::Ptr get_client(
        ObjectStoreClientType clientType) const;

  private:
    ObjectStorePluginHandler::Ptr m_plugin_handler;
};

using TierBackendRegistry = std::unordered_map<uint8_t, ObjectStoreClientType>;