#include "DistributedHsmService.h"

#include "HsmService.h"
#include "HttpClient.h"
#include "KeyValueStoreClient.h"

#include "Logger.h"

#define ON_ERROR(code, msg)                                                    \
    auto response = hestia::DistributedHsmServiceResponse::create(req);        \
    response->on_error({hestia::DistributedHsmServiceErrorCode::code, msg});   \
    return response;

namespace hestia {

DistributedHsmService::DistributedHsmService(
    DistributedHsmServiceConfig config,
    std::unique_ptr<HsmService> hsm_service,
    std::unique_ptr<HsmNodeService> node_service) :
    m_config(config),
    m_hsm_service(std::move(hsm_service)),
    m_node_service(std::move(node_service))
{
}

DistributedHsmService::Ptr DistributedHsmService::create(
    DistributedHsmServiceConfig config,
    std::unique_ptr<HsmService> hsm_service,
    KeyValueStoreClient* client)
{
    HsmNodeServiceConfig node_service_config;
    node_service_config.m_global_prefix = config.m_app_name;
    auto node_service = HsmNodeService::create(node_service_config, client);

    auto service = std::make_unique<DistributedHsmService>(
        config, std::move(hsm_service), std::move(node_service));

    if (config.m_is_server) {
        service->register_self();
    }
    return service;
}

DistributedHsmService::Ptr DistributedHsmService::create(
    DistributedHsmServiceConfig config,
    std::unique_ptr<HsmService> hsm_service,
    HttpClient* client)
{
    HsmNodeServiceConfig node_service_config;
    node_service_config.m_global_prefix = config.m_app_name;
    node_service_config.m_endpoint = config.m_controller_address + "/api/v1";
    auto node_service = HsmNodeService::create(node_service_config, client);

    auto service = std::make_unique<DistributedHsmService>(
        config, std::move(hsm_service), std::move(node_service));

    if (config.m_is_server) {
        service->register_self();
    }
    return service;
}

DistributedHsmService::~DistributedHsmService() {}

const DistributedHsmServiceConfig& DistributedHsmService::get_self_config()
    const
{
    return m_config;
}

DistributedHsmServiceResponse::Ptr DistributedHsmService::register_self()
{
    // Check if the node exists already
    Metadata query;
    query.set_item("tag", m_config.m_self.m_tag);
    LOG_INFO(
        "Checking for pre-registered endpoint with tag: "
        << m_config.m_self.m_tag);
    auto exists_response = m_node_service->make_request(query);
    if (!exists_response->ok()) {
        throw std::runtime_error(
            "Failed to check for pre-existing tag: "
            + exists_response->get_error().to_string());
    }

    if (!exists_response->items().empty()) {
        LOG_INFO(
            "Found endpoint with id: " << exists_response->items()[0].id());
        m_config.m_self.m_id = exists_response->items()[0].id();
    }
    else {
        LOG_INFO("Pre-existing endpoint not found - will request new one.");
    }

    DistributedHsmServiceRequest req(
        m_config.m_self, DistributedHsmServiceRequestMethod::PUT);
    auto put_response = put(req);
    if (!put_response->ok()) {
        throw std::runtime_error(
            "Failed to register node: "
            + put_response->get_error().to_string());
    }
    return put_response;
}

HsmService* DistributedHsmService::get_hsm_service()
{
    return m_hsm_service.get();
}

DistributedHsmServiceResponse::Ptr DistributedHsmService::make_request(
    const DistributedHsmServiceRequest& req) const noexcept
{
    switch (req.method()) {
        case DistributedHsmServiceRequestMethod::GET:
            return get(req);
        case DistributedHsmServiceRequestMethod::PUT:
            return put(req);
        case DistributedHsmServiceRequestMethod::LIST:
            return list(req);
        default:
            return nullptr;
    }
}

DistributedHsmServiceResponse::Ptr DistributedHsmService::get(
    const DistributedHsmServiceRequest& req) const
{
    LOG_INFO("Calling Node service multi get");
    const auto get_response =
        m_node_service->make_request(CrudMethod::MULTI_GET);
    if (!get_response->ok()) {
        ON_ERROR(
            ERROR, "Error in DistributedHsmService::get: "
                       + get_response->get_error().to_string());
    }

    auto response     = DistributedHsmServiceResponse::create(req);
    response->items() = get_response->items();
    LOG_INFO("Finished Node service multi get");
    return response;
}

DistributedHsmServiceResponse::Ptr DistributedHsmService::list(
    const DistributedHsmServiceRequest& req) const
{
    LOG_INFO("Calling Node service list");
    const auto get_response =
        m_node_service->make_request(CrudMethod::MULTI_GET);
    if (!get_response->ok()) {
        ON_ERROR(
            ERROR, "Error in DistributedHsmService::get: "
                       + get_response->get_error().to_string());
    }

    auto response = DistributedHsmServiceResponse::create(req);
    if (auto backend_query = req.query().get_item("backend");
        !backend_query.empty()) {
        std::vector<HsmNode> matches;
        for (const auto& node : get_response->items()) {
            for (const auto& backend : node.m_backends) {
                if (backend.m_identifier == backend_query) {
                    matches.push_back(node);
                    break;
                }
            }
        }
        response->items() = matches;
    }
    else {
        response->items() = get_response->items();
    }
    LOG_INFO("Finished Node service list");
    return response;
}

DistributedHsmServiceResponse::Ptr DistributedHsmService::put(
    const DistributedHsmServiceRequest& req) const
{
    LOG_INFO("Calling Node service put");

    CrudRequest<HsmNode> request(req.item(), CrudMethod::PUT);
    if (req.item().m_id.empty() && m_config.m_self.m_is_controller) {
        LOG_INFO("Requesting id generation");
        request.set_generate_id(true);
    }

    const auto put_response = m_node_service->make_request(request);
    if (!put_response->ok()) {
        ON_ERROR(
            ERROR, "Error in DistributedHsmService::put: "
                       + put_response->get_error().to_string());
    }

    auto response = DistributedHsmServiceResponse::create(req);
    LOG_INFO("Finished Node service put");
    return response;
}
}  // namespace hestia