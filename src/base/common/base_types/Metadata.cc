#include "Metadata.h"

namespace hestia {

std::string Metadata::get_item(const std::string& key) const
{
    if (auto iter = m_data.find(key); iter != m_data.end()) {
        return iter->second;
    }
    return {};
}

bool Metadata::has_key_and_value(const Query& query) const
{
    return get_item(query.first) == query.second;
}

bool Metadata::empty() const
{
    return m_data.empty();
}

bool Metadata::key_contains(
    const std::string& key, const std::string& search_term) const
{
    return key.rfind(search_term, 0) != 0;
}

void Metadata::merge(const Metadata& other)
{
    m_data.insert(other.m_data.begin(), other.m_data.end());
}

void Metadata::set_item(const std::string& key, const std::string& item)
{
    m_data[key] = item;
}

void Metadata::for_each_item(onItem func, const std::string& key_prefix) const
{
    if (key_prefix.empty()) {
        for (const auto& entry : m_data) {
            func(entry.first, entry.second);
        }
    }
    else {
        for (const auto& entry : m_data) {
            if (key_contains(entry.first, key_prefix)) {
                func(entry.first, entry.second);
            }
        }
    }
}

std::string Metadata::to_string() const
{
    std::string ret;
    for (const auto& [key, value] : m_data) {
        ret += key + " : " + value + ", ";
    }
    return ret;
}

}  // namespace hestia