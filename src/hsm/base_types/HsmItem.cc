#include "HsmItem.h"

namespace hestia {

HsmItem::HsmItem(HsmItem::Type type) : m_hsm_type(type) {}

HsmItem::Type HsmItem::get_hsm_type() const
{
    return m_hsm_type;
}

std::string HsmItem::to_name(Type type)
{
    switch (type) {
        case Type::DATASET:
            return dataset_name;
        case Type::OBJECT:
            return hsm_object_name;
        case Type::ACTION:
            return hsm_action_name;
        case Type::NAMESPACE:
            return namespace_name;
        case Type::TIER:
            return tier_name;
        case Type::EXTENT:
            return tier_extents_name;
        case Type::METADATA:
            return user_metadata_name;
        case Type::OBJECT_STORE_BACKEND:
            return object_store_backend_name;
        case Type::NODE:
            return hsm_node_name;
        case Type::UNKNOWN:
            return "";
        default:
            return "";
    }
}

HsmItem::Type HsmItem::from_name(const std::string& type_name)
{
    if (type_name == dataset_name) {
        return Type::DATASET;
    }
    else if (type_name == hsm_object_name) {
        return Type::OBJECT;
    }
    else if (type_name == hsm_action_name) {
        return Type::ACTION;
    }
    else if (type_name == namespace_name) {
        return Type::NAMESPACE;
    }
    else if (type_name == tier_name) {
        return Type::TIER;
    }
    else if (type_name == tier_extents_name) {
        return Type::EXTENT;
    }
    else if (type_name == user_metadata_name) {
        return Type::METADATA;
    }
    else if (type_name == hsm_node_name) {
        return Type::NODE;
    }
    else if (type_name == object_store_backend_name) {
        return Type::OBJECT_STORE_BACKEND;
    }
    else {
        return Type::UNKNOWN;
    }
}

std::vector<std::string> HsmItem::get_hsm_subjects()
{
    std::vector<std::string> subjects;
    for (const auto& subject : HsmItem::get_all_items()) {
        subjects.push_back(HsmItem::to_name(subject));
    }
    return subjects;
}

std::array<HsmItem::Type, 9> HsmItem::get_all_items()
{
    return s_all_items;
}
}  // namespace hestia
