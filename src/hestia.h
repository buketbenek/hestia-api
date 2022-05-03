#include "types.h"

namespace hestia {

void create_object(const struct hsm_uint& oid, struct hsm_obj& obj);

int put(
    const struct hsm_uint oid,
    struct hsm_obj* obj,
    const bool is_overwrite,
    const void* buf,
    const std::size_t offset,
    const std::size_t length,
    const std::uint8_t tgt_tier);

int get(
    const struct hsm_uint oid,
    struct hsm_obj* obj,
    void* buf,
    const std::size_t off,
    const std::size_t len,
    const std::uint8_t src_tier,
    const std::uint8_t tgt_tier);

}  // namespace hestia
