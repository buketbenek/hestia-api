#include "MockMotrInterfaceImpl.h"

#include "InMemoryStreamSink.h"
#include "InMemoryStreamSource.h"

#include "Logger.h"

#include <stdexcept>

#define MAX_BLOCK_COUNT (200)

namespace hestia {

class IoContext{
	public:
    IoContext(int num_blocks, size_t block_size, bool alloc_io_buff)
    {
        prepare(num_blocks, block_size, alloc_io_buff);
    }

    ~IoContext() { free(); }

    int prepare(int num_blocks, size_t block_size, bool alloc_io_buff)
    {
        m_allocated_io_buffer = alloc_io_buff;

        /* allocate new I/O vec? */
        if (num_blocks != m_current_blocks
            || block_size != m_current_block_size) {
            if (m_current_blocks != 0) {
                free_data();
            }

            int rc = 0;
            if (m_allocated_io_buffer) {
		m_data.m_buffers.resize(num_blocks);
		m_data.m_counts.resize(num_blocks);
		memory_allocated=true;
		for(int i =0; i<num_blocks; i++){
			m_data.m_buffers[i] = new char[num_blocks];
		}
		return 0;
            }
            else {
		m_data.m_buffers.resize(num_blocks);
		m_data.m_counts.resize(num_blocks);
		return 0;
            }
            if (rc != 0) {
                return rc;
            }
        }

        /* allocate extents and attrs, if they are not already */
        if (num_blocks != m_current_blocks) {
            /* free previous vectors */
            if (m_current_blocks > 0) {
                free_index();
            }

            /* Allocate attr and extent list*/
	    m_ext.m_counts.resize(num_blocks);
	    m_ext.m_indices.resize(num_blocks);

        }

        m_current_blocks     = num_blocks;
        m_current_block_size = block_size;
        return 0;
    }

    int map(int num_blocks, size_t block_size, off_t offset, char* buff)
    {
        if (num_blocks == 0) {
            return -EINVAL;
        }

        for (int i = 0; i < num_blocks; i++) {
            m_ext.m_indices[i]       = offset + i * block_size;
            m_ext.m_counts[i] = block_size;

            /* we don't want any attributes */
            m_attr.m_counts[i] = 0;

            if (m_data.m_counts[i] == 0) {
                m_data.m_counts[i] = block_size;
            }
            /* check the allocated buffer has the right size */
            else if (m_data.m_counts[i] != block_size) {
                return -EINVAL;
            }

            /* map the user-provided I/O buffer */
            if (buff != nullptr) {
                m_data.m_buffers[i] = buff + i * block_size;
            }
        }
        return 0;
    }

    void free_data()
    {
        if (m_allocated_io_buffer) {
	for(std::size_t i =0; i<m_data.m_buffers.size(); i++){
                        delete[] m_data.m_buffers[i];
                }    
	m_data.clear();
	    memory_allocated=false;
        }
        else {
		m_data.clear();
        }
    }

    void free_index()
    {
        m_attr.clear();
        m_ext.clear();
    }

    void free()
    {
        free_data();
        free_index();
    }


    void to_buffer(WriteableBufferView& buffer, std::size_t block_size)
    {
        std::size_t buff_idx = 0;
        for (std::size_t i = 0; i < m_data.m_counts.size(); i++) {
            for (std::size_t j = 0; j < block_size; j++) {
                buffer.data()[buff_idx] = ((char*)m_data.m_buffers[i])[j];
                buff_idx++;
            }
        }
    }

	int read_blocks(char * stored)
	{
		for(std::size_t i=0; i<m_data.m_counts.size(); i++){
			int idx=m_ext.m_indices[i];
			for(std::size_t j=0; j<m_data.m_counts[i];j++){
				((char*)m_data.m_buffers[i])[j] = stored[idx + j];	
			}		
		}	
		return 0;
	}

    int m_current_blocks{0};
    size_t m_current_block_size{0};
    bool m_allocated_io_buffer{false};
    bool memory_allocated{false};
    
    struct mock::motr::IndexVec m_ext;
    struct mock::motr::BufferVec m_attr;
    struct mock::motr::BufferVec m_data;
};

class MotrObject {
  public:
    MotrObject(const hestia::Uuid& oid) : m_id(oid) {}

    ~MotrObject() = default;

    mock::motr::Id get_motr_id() const { return to_motr_id(m_id); }

    mock::motr::Obj* get_motr_obj() { return &m_handle; }

    static mock::motr::Id to_motr_id(const hestia::Uuid& id)
    {
        mock::motr::Id motr_id;
        motr_id.m_lo = id.m_lo;
        motr_id.m_hi = id.m_hi;
        return motr_id;
    }

    std::size_t m_size{0};
	int read_blocks()
    {
        if (!m_io_ctx) {
            m_io_ctx = std::make_unique<IoContext>(
                m_unread_block_count, m_block_size, true);
        }
        else {
            auto rc =
                m_io_ctx->prepare(m_unread_block_count, m_block_size, true);
            if (rc != 0) {
                return rc;
            }
        }
        auto rc = m_io_ctx->map(
            m_unread_block_count, m_block_size, m_start_offset, nullptr);
        if (rc != 0) {
            return rc;
        }
        return m_io_ctx->read_blocks(stored_data);
    }

    int read(WriteableBufferView& buffer, std::size_t length)
    {
        for (auto transfer_size = length; transfer_size > 0;
             transfer_size -= get_last_transfer_size()) {
            set_block_layout(transfer_size);

            if (transfer_size < m_min_block_size) {
                m_block_size = m_min_block_size;
            }
            if (auto rc = read_blocks(); rc != 0) {
                return rc;
            }
            if (transfer_size < m_min_block_size) {
                m_io_ctx->to_buffer(buffer, transfer_size);
            }
            else {
                m_io_ctx->to_buffer(buffer, m_block_size);
            }

            m_start_offset += get_last_transfer_size();
        }
        return 0;
    }

    void set_block_layout(std::size_t transfer_size)
    {
        m_unread_block_count = transfer_size / m_block_size;
        if (m_unread_block_count == 0) {
            m_unread_block_count = 1;
            m_block_size         = transfer_size;
        }
        else if (m_unread_block_count > MAX_BLOCK_COUNT) {
            m_unread_block_count = MAX_BLOCK_COUNT;
        }
    }

    std::size_t get_last_transfer_size() const
    {
        return m_unread_block_count * m_block_size;
    }


    hestia::Uuid m_id;
    mock::motr::Obj m_handle;
    char * stored_data;

    
        std::size_t m_total_size{0};
    int m_unread_block_count{0};
    std::size_t m_block_size{0};
    std::size_t m_min_block_size{0};
    std::size_t m_start_offset{0};
    std::unique_ptr<IoContext> m_io_ctx;

};

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
	/*
    MotrObject motr_obj(request.object().id());

    auto rc = m_hsm.m0hsm_create(
        motr_obj.get_motr_id(), motr_obj.get_motr_obj(), request.target_tier(),
        false);
    if (rc < 0) {
        const std::string msg =
            "Failed to create object: " + std::to_string(rc);
        LOG_ERROR(msg);
        throw std::runtime_error(msg);
    }

    rc = m_hsm.m0hsm_set_write_tier(
        motr_obj.get_motr_id(), request.target_tier());
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
            working_obj.get_motr_obj(), const_cast<void*>(buffer.as_void()),
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
    */ return;
}

void MockMotrInterfaceImpl::get(
    const HsmObjectStoreRequest& request,
    hestia::StorageObject& object,
    hestia::Stream* stream) const
{
/*
    (void)object;

    MotrObject motr_obj(request.object().id());
    motr_obj.m_size = request.object().m_size;

    auto rc = m_hsm.m0hsm_set_read_tier(
        motr_obj.get_motr_id(), request.source_tier());
    if (rc < 0) {
        const std::string msg =
            "Failed to set read tier: " + std::to_string(rc);
        LOG_ERROR(msg);
        throw std::runtime_error(msg);
    }

    auto source_func = [this, motr_obj](
                           WriteableBufferView& buffer,
                           std::size_t offset) -> InMemoryStreamSource::Status {
        if (offset >= motr_obj.m_size) {
            return {true, 0};
        }

        auto read_size = buffer.length();
        if (offset + buffer.length() > motr_obj.m_size) {
            read_size = motr_obj.m_size - offset;
        }

        auto rc = m_hsm.m0hsm_read(
            motr_obj.get_motr_id(), buffer.as_void(), read_size, offset);
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
    */ return;
}

void MockMotrInterfaceImpl::remove(const HsmObjectStoreRequest& request) const
{
	/*
    MotrObject motr_obj(request.object().id());

    std::size_t offset{0};
    std::size_t length{IMotrInterfaceImpl::max_obj_length};
    mock::motr::hsm_rls_flags flags =
        mock::motr::hsm_rls_flags::HSM_KEEP_LATEST;
    auto rc = m_hsm.m0hsm_release(
        motr_obj.get_motr_id(), request.source_tier(), offset, length, flags);
    if (rc < 0) {
        std::string msg = "Error in  m0hsm_release" + std::to_string(rc);
        LOG_ERROR(msg);
        throw std::runtime_error(msg);
    }
    */ return;
}

void MockMotrInterfaceImpl::copy(const HsmObjectStoreRequest& request) const
{
	/*
    MotrObject motr_obj(request.object().id());
    std::size_t length = request.extent().m_length;
    if (length == 0) {
        length = IMotrInterfaceImpl::max_obj_length;
    }

    mock::motr::Hsm::hsm_cp_flags flags =
        mock::motr::Hsm::hsm_cp_flags::HSM_KEEP_OLD_VERS;
    auto rc = m_hsm.m0hsm_copy(
        motr_obj.get_motr_id(), request.source_tier(), request.target_tier(),
        request.extent().m_offset, length, flags);
    if (rc < 0) {
        std::string msg = "Error in  m0hsm_copy" + std::to_string(rc);
        LOG_ERROR(msg);
        throw std::runtime_error(msg);
    }
    */ return;
}

void MockMotrInterfaceImpl::move(const HsmObjectStoreRequest& request) const
{
	/*
    MotrObject motr_obj(request.object().id());
    std::size_t length = request.extent().m_length;
    if (length == 0) {
        length = IMotrInterfaceImpl::max_obj_length;
    }

    mock::motr::Hsm::hsm_cp_flags flags =
        mock::motr::Hsm::hsm_cp_flags::HSM_MOVE;
    auto rc = m_hsm.m0hsm_copy(
        motr_obj.get_motr_id(), request.source_tier(), request.target_tier(),
        request.extent().m_offset, length, flags);
    if (rc < 0) {
        std::string msg = "Error in  m0hsm_copy - move " + std::to_string(rc);
        LOG_ERROR(msg);
        throw std::runtime_error(msg);
    }
    */ return;
}
}  // namespace hestia
