#include "InMemoryHsmObjectStoreClient.h"

#include "RequestException.h"

#include "ErrorUtils.h"
#include "Logger.h"

#include <cassert>
#include <stdexcept>

namespace hestia {

InMemoryObjectStoreClientConfig::InMemoryObjectStoreClientConfig() :
    SerializeableWithFields("in_memory_hsm_object_store_client")
{
    init();
}

InMemoryObjectStoreClientConfig::InMemoryObjectStoreClientConfig(
    const InMemoryObjectStoreClientConfig& other) :
    SerializeableWithFields(other)
{
    *this = other;
}

InMemoryObjectStoreClientConfig& InMemoryObjectStoreClientConfig::operator=(
    const InMemoryObjectStoreClientConfig& other)
{
    if (this != &other) {
        SerializeableWithFields::operator=(other);
        init();
    }
    return *this;
}

void InMemoryObjectStoreClientConfig::init() {}

InMemoryHsmObjectStoreClient::InMemoryHsmObjectStoreClient()
{
    LOG_INFO("Created");
}

InMemoryHsmObjectStoreClient::~InMemoryHsmObjectStoreClient()
{
    LOG_INFO("Destroyed");
}

std::string InMemoryHsmObjectStoreClient::get_registry_identifier()
{
    return "hestia::InMemoryHsmObjectStoreClient";
}

InMemoryHsmObjectStoreClient::Ptr InMemoryHsmObjectStoreClient::create()
{
    return std::make_unique<InMemoryHsmObjectStoreClient>();
}

void InMemoryHsmObjectStoreClient::initialize(
    const std::string& id,
    const std::string& cache_path,
    const Dictionary& config_data)
{
    InMemoryObjectStoreClientConfig config;
    config.deserialize(config_data);
    do_initialize(id, cache_path, config);
}

void InMemoryHsmObjectStoreClient::do_initialize(
    const std::string& id,
    const std::string&,
    const InMemoryObjectStoreClientConfig&)
{
    m_id = id;

    for (const auto& tier_id : m_tier_names) {
        m_tiers[tier_id] = std::make_unique<InMemoryObjectStoreClient>();
    }
}

InMemoryObjectStoreClient* InMemoryHsmObjectStoreClient::get_tier_client(
    uint8_t tier) const
{
    if (auto iter = m_tiers.find(std::to_string(tier)); iter != m_tiers.end()) {
        return iter->second.get();
    }
    throw std::runtime_error(
        SOURCE_LOC() + " | Client not found for tier " + std::to_string(tier)
        + " - it is missing from the input config.");
}

std::string InMemoryHsmObjectStoreClient::dump() const
{
    std::string output;
    for (const auto& [name, client] : m_tiers) {
        output += "Tier: " + name + '\n';
        output += client->dump() + '\n';
    }
    return output;
}

void InMemoryHsmObjectStoreClient::put(
    const HsmObjectStoreRequest& request, Stream* stream) const
{
    assert(stream != nullptr);

    auto client = get_tier_client(request.target_tier());

    if (const auto response = client->make_request(
            HsmObjectStoreRequest::to_base_request(request), stream);
        !response->ok()) {
        const std::string msg =
            "Error in file client PUT: " + response->get_error().to_string();
        throw RequestException<HsmObjectStoreError>(
            {HsmObjectStoreErrorCode::ERROR, msg});
    }
}

void InMemoryHsmObjectStoreClient::get(
    const HsmObjectStoreRequest& request, StorageObject&, Stream* stream) const
{
    assert(stream != nullptr);

    LOG_INFO("Getting data");
    auto client = get_tier_client(request.source_tier());

    if (const auto response = client->make_request(
            HsmObjectStoreRequest::to_base_request(request), stream);
        !response->ok()) {
        const std::string msg = "Error in file client data GET: "
                                + response->get_error().to_string();
        throw RequestException<HsmObjectStoreError>(
            {HsmObjectStoreErrorCode::ERROR, msg});
    }
}

void InMemoryHsmObjectStoreClient::remove(
    const HsmObjectStoreRequest& request) const
{
    auto client = get_tier_client(request.source_tier());

    if (const auto response = client->make_request(
            HsmObjectStoreRequest::to_base_request(request));
        !response->ok()) {
        const std::string msg =
            "Error in file client REMOVE: " + response->get_error().to_string();
        throw RequestException<HsmObjectStoreError>(
            {HsmObjectStoreErrorCode::ERROR, msg});
    }
}

void InMemoryHsmObjectStoreClient::copy(
    const HsmObjectStoreRequest& request) const
{
    auto source_client = get_tier_client(request.source_tier());
    auto target_client = get_tier_client(request.target_tier());

    source_client->migrate(request.object().id(), target_client, false);
}

void InMemoryHsmObjectStoreClient::move(
    const HsmObjectStoreRequest& request) const
{
    auto source_client = get_tier_client(request.source_tier());
    auto target_client = get_tier_client(request.target_tier());

    source_client->migrate(request.object().id(), target_client, true);
}
}  // namespace hestia
