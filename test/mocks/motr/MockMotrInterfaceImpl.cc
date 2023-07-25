#include "MockMotrInterfaceImpl.h"
#include "MockIOContext.h"
#include "MockMotrObject.h"


#include "InMemoryStreamSink.h"
#include "InMemoryStreamSource.h"

#include "Logger.h"

#include <stdexcept>

#define MAX_BLOCK_COUNT (200)

namespace hestia {
    
void MockMotrInterfaceImpl::initialize(const MotrConfig& config)
{
    static constexpr int mo_uber_realm = 0;

    m_hsm.motr()->m0_client_init(&m_client, nullptr, true);
    mock::motr::Id realm_id{mo_uber_realm, 0};
    m_hsm.motr()->m0_container_init(
        &m_container, nullptr, &realm_id, &m_client);

    std::size_t pool_count{0};
    LOG_INFO("Initializing with: " << config.m_tier_info.size() << " pools.");
    for (const auto& entry : config.m_tier_info) {
        (void)entry;
        m_client.add_pool({pool_count, 0});
        pool_count++;
    }

    initialize_hsm(config.m_tier_info);
}

void MockMotrInterfaceImpl::initialize_hsm(
    const std::vector<MotrHsmTierInfo>& tier_info)
{
    std::vector<hestia::Uuid> pool_fids;
    std::size_t pool_count{0};
    for (const auto& entry : tier_info) {
        (void)entry;
        pool_fids.push_back({pool_count, 0});
        pool_count++;
    }

    mock::motr::HsmOptions hsm_options;
    hsm_options.m_pool_fids = pool_fids;

    m_hsm.m0hsm_init(&m_client, &m_container.m_co_realm, &hsm_options);
}

void MockMotrInterfaceImpl::put(
    const HsmObjectStoreRequest& request, hestia::Stream* stream) const
{
    auto motr_obj = std::make_shared<MockMotrObject>(request.object().id());

    auto rc = m_hsm.m0hsm_create(
        motr_obj->m_motr_id, &(motr_obj->m_handle), request.target_tier(),
        false);
    if (rc < 0) {
        const std::string msg =
            "Failed to create object: " + std::to_string(rc);
        LOG_ERROR(msg);
        throw std::runtime_error(msg);
    }

    rc = m_hsm.m0hsm_set_write_tier(
        motr_obj->m_motr_id, request.target_tier());
    if (rc < 0) {
        const std::string msg =
            "Failed to set write tier: " + std::to_string(rc);
        LOG_ERROR(msg);
        throw std::runtime_error(msg);
    }

    auto sink_func = [this, motr_obj](
                         const ReadableBufferView& buffer,
                         std::size_t offset) -> InMemoryStreamSink::Status {
        LOG_INFO(
            "Writing buffer with size: " << buffer.length() << " and offset "
                                         << offset);

        auto working_obj = motr_obj;
        auto rc          = m_hsm.m0hsm_pwrite(
            &(working_obj->m_handle), const_cast<void*>(buffer.as_void()),
            buffer.length(), offset);
        if (rc < 0) {
            std::string msg =
                "Error writing buffer at offset " + std::to_string(offset);
            LOG_ERROR(msg);
            return {false, 0};
        }
        return {true, buffer.length()};
    };

    auto sink = InMemoryStreamSink::create(sink_func);
    stream->set_sink(std::move(sink));
}

void MockMotrInterfaceImpl::get(
    const HsmObjectStoreRequest& request,
    hestia::StorageObject& object,
    hestia::Stream* stream) const
{
    (void)object;

    auto motr_obj = std::make_shared<MockMotrObject>(request.object().id());
    motr_obj->m_total_size = request.object().m_size;

    auto rc = m_hsm.m0hsm_set_read_tier(
        motr_obj->m_motr_id, request.source_tier());
    if (rc < 0) {
        const std::string msg =
            "Failed to set read tier: " + std::to_string(rc);
        LOG_ERROR(msg);
        throw std::runtime_error(msg);
    }

    auto source_func = [this, motr_obj](
                           WriteableBufferView& buffer,
                           std::size_t offset) -> InMemoryStreamSource::Status {
        if (offset >= motr_obj->m_total_size) {
            return {true, 0};
        }

        auto read_size = buffer.length();
        if (offset + buffer.length() > motr_obj->m_total_size) {
            read_size = motr_obj->m_total_size - offset;
        }

        auto rc = m_hsm.m0hsm_read(
            motr_obj->m_motr_id, buffer.as_void(), read_size, offset);
        if (rc < 0) {
            std::string msg =
                "Error writing buffer at offset " + std::to_string(offset);
            LOG_ERROR(msg);
            return {false, 0};
        }
        return {true, read_size};
    };

    auto source = hestia::InMemoryStreamSource::create(source_func);
    stream->set_source(std::move(source));
}

void MockMotrInterfaceImpl::remove(const HsmObjectStoreRequest& request) const
{
    auto motr_obj = std::make_shared<MockMotrObject>(request.object().id());

    std::size_t offset{0};
    std::size_t length{IMotrInterfaceImpl::max_obj_length};
    mock::motr::hsm_rls_flags flags =
        mock::motr::hsm_rls_flags::HSM_KEEP_LATEST;
    auto rc = m_hsm.m0hsm_release(
        motr_obj->m_motr_id, request.source_tier(), offset, length, flags);
    if (rc < 0) {
        std::string msg = "Error in  m0hsm_release" + std::to_string(rc);
        LOG_ERROR(msg);
        throw std::runtime_error(msg);
    }
}

void MockMotrInterfaceImpl::copy(const HsmObjectStoreRequest& request) const
{
    auto motr_obj = std::make_shared<MockMotrObject>(request.object().id());
    std::size_t length = request.extent().m_length;
    if (length == 0) {
        length = IMotrInterfaceImpl::max_obj_length;
    }

    mock::motr::Hsm::hsm_cp_flags flags =
        mock::motr::Hsm::hsm_cp_flags::HSM_KEEP_OLD_VERS;
    auto rc = m_hsm.m0hsm_copy(
        motr_obj->m_motr_id, request.source_tier(), request.target_tier(),
        request.extent().m_offset, length, flags);
    if (rc < 0) {
        std::string msg = "Error in  m0hsm_copy" + std::to_string(rc);
        LOG_ERROR(msg);
        throw std::runtime_error(msg);
    }
}

void MockMotrInterfaceImpl::move(const HsmObjectStoreRequest& request) const
{
    auto motr_obj = std::make_shared<MockMotrObject>(request.object().id());
    std::size_t length = request.extent().m_length;
    if (length == 0) {
        length = IMotrInterfaceImpl::max_obj_length;
    }

    mock::motr::Hsm::hsm_cp_flags flags =
        mock::motr::Hsm::hsm_cp_flags::HSM_MOVE;
    auto rc = m_hsm.m0hsm_copy(
        motr_obj->m_motr_id, request.source_tier(), request.target_tier(),
        request.extent().m_offset, length, flags);
    if (rc < 0) {
        std::string msg = "Error in  m0hsm_copy - move " + std::to_string(rc);
        LOG_ERROR(msg);
        throw std::runtime_error(msg);
    }
}
}  // namespace hestia