#pragma once

#include "KeyValueStoreClient.h"

#include <filesystem>

namespace hestia {
class FileKeyValueStoreClient : public KeyValueStoreClient {
  public:
    FileKeyValueStoreClient();

    ~FileKeyValueStoreClient() = default;

    void initialize(const Metadata& config) override;

  private:
    bool string_exists(const std::string& key) const override;

    void string_get(const std::string& key, std::string& value) const override;

    void string_set(
        const std::string& key, const std::string& value) const override;

    void string_remove(const std::string& key) const override;

    void set_add(
        const std::string& key, const std::string& value) const override;

    void set_list(
        const std::string& key, std::vector<std::string>& value) const override;

    void set_remove(
        const std::string& key, const std::string& value) const override;

    std::filesystem::path m_store{"kv_store"};
};
}  // namespace hestia