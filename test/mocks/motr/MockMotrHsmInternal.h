#pragma once

#include "CompositeLayer.h"
#include "MockMotr.h"

#include <functional>
#include <string>
#include <vector>

namespace hestia::mock::motr {

enum class hsm_rls_flags { HSM_KEEP_LATEST };

struct IoContext {
    IndexVec m_extents;
    BufferVec m_data;
    std::vector<char> m_stage;
};

class HsmInternal {
  public:
    static uint32_t hsm_priority(uint32_t generation, uint8_t tier_idx);

    static Id hsm_subobj_id(Id id, uint32_t gen, uint8_t tier_idx);

    static uint32_t hsm_prio2gen(uint32_t priority);

    static uint8_t hsm_prio2tier(uint32_t priority);

    hestia::Uuid* hsm_tier2pool(uint8_t tier_idx) const;

    int create_obj(Id id, Obj* obj, bool close_entity, uint8_t tier_idx) const;

    static void layout_top_prio(
        Layout* layout, int32_t* max_gen, Id* max_gen_id, uint8_t* top_tier);

    static int check_top_layer_writable(Layout* layout, int max_prio, int tier);

    int delete_obj_set_parent_layout(
        Id id, Id parent_id, Layout* parent_layout) const;

    int layer_clean(Id parent_id, Layout* layout, CompositeLayer* layer) const;

    int m0hsm_release_maxgen(
        Id id,
        uint8_t tier,
        int max_gen,
        off_t offset,
        size_t len,
        hsm_rls_flags flags,
        bool user_display) const;

    int layout_get(Id id, Layout** layout) const;

    int layout_set(Id id, Layout* layout) const;

    int layout_add_top_layer(Id id, Layout* layout, uint8_t tier) const;

    int layout_layer_clean(Id parent_id, Layout* layout, Id subobj_id) const;

    int layer_check_clean(
        Id parent_id, Layout* layout, CompositeLayer* layer) const;

    CompositeLayer* layer_get_by_prio(Layout* layout, int prio) const;

    int top_layer_add_read_extent(Obj* obj, const hestia::Extent& ext) const;

    int copy_extent_data(Id src_id, Id tgt_id, hestia::Extent* range) const;

    int layer_extent_add(
        Id subobjid,
        const hestia::Extent& ext,
        bool write,
        bool overwrite) const;

    int layer_extent_del(Id subobjid, off_t off, bool write) const;

    using layer_match_callback = std::function<int(
        void* cb_arg,
        Layout* layout,
        CompositeLayer* layer,
        hestia::Extent* match,
        bool* stop)>;
    static int match_layer_foreach(
        Layout* layout,
        uint8_t tier,
        hestia::Extent* ext,
        layer_match_callback cb,
        void* cb_arg,
        bool stop_on_error);

    static int check_min_gen_exists(
        Layout* layout,
        hestia::Extent* ext,
        int gen,
        CompositeLayer* except_layer);

    static int prepare_io_ctx(
        IoContext& ctx, int blocks, size_t block_size, bool alloc_io_buff);

    static int map_io_ctx(
        IoContext& ctx, int blocks, size_t b_size, off_t offset, char* buff);

    int write_blocks(const Obj& obj, const IoContext& ctx) const;

    int read_blocks(const Obj& obj, const IoContext& ctx) const;

    void print_layout(std::string& sink, Layout* layout, bool details) const;

    void print_layer(
        std::string& sink, CompositeLayer* layer, bool details) const;

    const Motr* motr() const { return &m_motr_context; }

    void set_pools(const std::vector<hestia::Uuid>& pools) const
    {
        m_hsm_pools = pools;
    }

    void set_realm(Realm* realm) const { m_uber_realm = realm; }

    void set_client(Client* client) const { m_client = client; }

    Client* client() const { return m_client; }

    Realm* get_realm() const { return m_uber_realm; }

    struct ReleaseContext {
        int found{0};
        Id obj_id;
        uint8_t tier{0};
        int max_gen{0};  /* max generation to release */
        int max_tier{0}; /* max tier to release from */
    };

    int on_layer_match_for_release(
        ReleaseContext* ctx,
        Layout* layout,
        CompositeLayer* layer,
        hestia::Extent* match,
        bool* stop) const;

    static constexpr int hsm_any_tier{UINT8_MAX};
    static constexpr std::size_t default_block_size{400};

  private:
    int obj_layout_get(Obj* obj, Layout** layout) const;
    int obj_layout_set(Obj* obj, Layout* layout) const;

    std::size_t m_io_block_size{default_block_size};

    mutable Client* m_client{nullptr};
    mutable Realm* m_uber_realm{nullptr};
    Motr m_motr_context;
    mutable std::vector<hestia::Uuid> m_hsm_pools;
};
}  // namespace hestia::mock::motr