#include "KeyValueCrudClient.h"

#include "IdGenerator.h"
#include "KeyValueStoreClient.h"
#include "KeyValueStoreRequest.h"
#include "StringAdapter.h"
#include "TimeProvider.h"

#include "CrudService.h"
#include "ErrorUtils.h"
#include "Logger.h"
#include "RequestException.h"

#include <cassert>
#include <iostream>

namespace hestia {
KeyValueCrudClient::KeyValueCrudClient(
    const CrudClientConfig& config,
    AdapterCollectionPtr adapters,
    KeyValueStoreClient* client,
    IdGenerator* id_generator,
    TimeProvider* time_provider) :
    CrudClient(config, std::move(adapters), id_generator, time_provider),
    m_client(client)
{
}

KeyValueCrudClient::~KeyValueCrudClient() {}

void KeyValueCrudClient::process_fields(Model* item, Fields& fields) const
{
    Model::VecForeignKeyContext foregin_keys;
    item->get_foreign_key_fields(foregin_keys);
    fields.m_foreign_key.push_back(foregin_keys);

    VecKeyValuePair one_to_one_defaults;
    item->get_default_create_one_to_one_fields(one_to_one_defaults);
    fields.m_one_to_one.push_back(one_to_one_defaults);

    std::vector<Model::TypeIdsPair> many_to_many;
    item->get_many_to_many_fields(many_to_many);
    fields.m_many_many.push_back(many_to_many);

    if (auto parent_id = item->get_parent_id(); !parent_id.empty()) {
        fields.m_parent_ids.push_back(parent_id);
    }
}

void KeyValueCrudClient::process_items(
    const CrudRequest& request,
    std::vector<std::string>& ids,
    Fields& fields,
    Dictionary& content) const
{
    std::size_t count{0};
    for (const auto& item : request.items()) {
        auto id = item->get_primary_key();
        if (id.empty()) {
            id = generate_id(item->name());
        }
        ids.push_back(id);

        SerializeableWithFields::VecIndexField index;
        item->get_index_fields(index);
        fields.m_index.push_back(index);

        process_fields(item.get(), fields);

        auto item_dict = std::make_unique<Dictionary>();
        get_adapter(CrudAttributes::Format::JSON)
            ->to_dict(request.items(), *item_dict, {}, count);
        content.add_sequence_item(std::move(item_dict));
        count++;
    }
}

void KeyValueCrudClient::process_ids(
    const CrudRequest& request,
    std::vector<std::string>& ids,
    Fields& fields,
    const Dictionary& attributes,
    Dictionary& content) const
{
    std::size_t count{0};
    for (const auto& crud_id : request.get_ids()) {
        std::string id = crud_id.get_primary_key();
        if (id.empty()) {
            id = generate_id(crud_id.get_name());
        }
        ids.push_back(id);

        auto base_item = m_adapters->get_model_factory()->create();
        if (!crud_id.get_name().empty()) {
            base_item->set_name(crud_id.get_name());
        }
        if (!crud_id.get_parent_name().empty()) {
            fields.m_parent_names.push_back(crud_id.get_parent_name());
        }
        if (!crud_id.get_parent_primary_key().empty()) {
            fields.m_parent_ids.push_back(crud_id.get_parent_primary_key());
        }

        if (!attributes.is_empty()) {
            if (attributes.get_type() == Dictionary::Type::MAP) {
                base_item->deserialize(attributes);
            }
            else if (
                attributes.get_type() == Dictionary::Type::SEQUENCE
                && attributes.get_sequence().size()
                       == request.get_ids().size()) {
                base_item->deserialize(*attributes.get_sequence()[count]);
            }
        }

        SerializeableWithFields::VecIndexField index;
        base_item->get_index_fields(index);
        fields.m_index.push_back(index);

        process_fields(base_item.get(), fields);

        VecModelPtr base_items;
        base_items.push_back(std::move(base_item));

        auto item_dict = std::make_unique<Dictionary>();
        get_adapter(CrudAttributes::Format::JSON)
            ->to_dict(base_items, *item_dict, {});
        content.add_sequence_item(std::move(item_dict));
        count++;
    }
}

void KeyValueCrudClient::process_empty(
    std::vector<std::string>& ids,
    Fields& fields,
    const Dictionary& attributes,
    Dictionary& content) const
{
    ids.push_back(generate_id(""));

    auto base_item = m_adapters->get_model_factory()->create();

    if (!attributes.is_empty()
        && attributes.get_type() == Dictionary::Type::MAP) {
        base_item->deserialize(attributes);
    }

    SerializeableWithFields::VecIndexField index;
    base_item->get_index_fields(index);
    fields.m_index.push_back(index);

    process_fields(base_item.get(), fields);

    VecModelPtr base_items;
    base_items.push_back(std::move(base_item));

    auto item_dict = std::make_unique<Dictionary>();
    get_adapter(CrudAttributes::Format::JSON)
        ->to_dict(base_items, *item_dict, {});
    content.add_sequence_item(std::move(item_dict));
}

void KeyValueCrudClient::prepare_create_keys(
    std::vector<KeyValuePair>& string_set_kv_pairs,
    std::vector<KeyValuePair>& set_add_kv_pairs,
    std::vector<std::string>& ids,
    const Fields& fields,
    const Dictionary& content,
    const Dictionary& create_context_dict,
    const std::string& primary_key_name) const
{
    std::size_t count{0};
    for (const auto& id : ids) {
        auto item_dict = content.get_sequence()[count].get();
        item_dict->merge(create_context_dict);

        auto primary_key_dict =
            std::make_unique<Dictionary>(Dictionary::Type::SCALAR);
        primary_key_dict->set_scalar(id);
        item_dict->set_map_item(primary_key_name, std::move(primary_key_dict));

        for (const auto& [name, id] :
             fields.m_foreign_key_id_replacements[count]) {
            auto fk_item = item_dict->get_map_item(name);
            fk_item->set_map({{"id", id}});
        }

        std::string content_body;
        get_adapter(CrudAttributes::Format::JSON)
            ->dict_to_string(*item_dict, content_body);

        string_set_kv_pairs.emplace_back(get_item_key(id), content_body);

        for (const auto& [scope, name, value] : fields.m_index[count]) {
            std::string field_key;
            if (scope == BaseField::IndexScope::GLOBAL) {
                field_key = name;
            }
            else {
                std::string parent_id;
                if (fields.m_parent_ids.size() == fields.m_index.size()) {
                    parent_id = fields.m_parent_ids[count];
                }
                field_key = parent_id.empty() ? name : parent_id + "::" + name;
            }
            string_set_kv_pairs.emplace_back(
                get_field_key(field_key, value), id);
        }

        for (const auto& field_context : fields.m_foreign_key[count]) {
            const auto foreign_key =
                m_config.m_prefix + ":" + field_context.m_type + ":"
                + field_context.m_id + ":" + m_adapters->get_type() + "s";
            set_add_kv_pairs.emplace_back(foreign_key, id);
        }

        for (const auto& [type, ids] : fields.m_many_many[count]) {
            for (const auto& item_id : ids) {
                const auto many_many_key = m_config.m_prefix + ":" + type + ":"
                                           + item_id + ":"
                                           + m_adapters->get_type() + "s";
                set_add_kv_pairs.emplace_back(many_many_key, id);
            }
        }

        set_add_kv_pairs.emplace_back(get_set_key(), id);
        count++;
    }
}

void KeyValueCrudClient::create(
    const CrudRequest& crud_request,
    CrudResponse& crud_response,
    bool record_modified_attrs)
{
    Dictionary attributes_dict;
    if (crud_request.get_attributes().has_content()) {
        auto adapter = get_adapter(crud_request.get_attributes().get_format());
        adapter->dict_from_string(
            crud_request.get_attributes().get_buffer(), attributes_dict);
    }

    auto content = std::make_unique<Dictionary>(Dictionary::Type::SEQUENCE);
    auto item_template = m_adapters->get_model_factory()->create();

    std::vector<std::string> ids;
    std::vector<std::string> parent_ids;
    std::vector<std::string> parent_names;
    Fields working_fields;

    if (crud_request.has_items()) {
        process_items(crud_request, ids, working_fields, *content);
    }
    else if (!crud_request.get_ids().empty()) {
        process_ids(
            crud_request, ids, working_fields, attributes_dict, *content);
    }
    else {
        process_empty(ids, working_fields, attributes_dict, *content);
    }

    // Loop through foreign keys - if it has an id check that it exists.
    // If it has no id, but we have a parent id try to use that.
    // If it still has no id, but we have a parent name try to use that.
    // If it stillll has no id try to create a default entry.
    // If it can't create a default entry then fail.
    std::size_t count{0};
    for (auto& item_foreign_keys : working_fields.m_foreign_key) {
        VecKeyValuePair id_replacements;
        for (auto& field_context : item_foreign_keys) {
            if (field_context.m_id.empty()) {
                const auto parent_type = item_template->get_parent_type();
                if (parent_type.empty()) {
                    THROW_WITH_SOURCE_LOC(
                        "Have empty foreign key id and missing parent type");
                }

                if (working_fields.m_parent_ids.size()
                    == working_fields.m_foreign_key.size()) {
                    field_context.m_id = working_fields.m_parent_ids[count];
                    id_replacements.push_back(
                        {field_context.m_name,
                         working_fields.m_parent_ids[count]});
                }
                else {
                    auto default_parent_id = get_default_parent_id(parent_type);
                    if (default_parent_id.empty()) {
                        get_or_create_default_parent(
                            parent_type, crud_request.get_user_context().m_id);
                        default_parent_id = get_default_parent_id(parent_type);
                    }
                    field_context.m_id = default_parent_id;
                    id_replacements.push_back(
                        {field_context.m_name, default_parent_id});
                    working_fields.m_parent_ids.push_back(default_parent_id);
                }
            }
        }
        working_fields.m_foreign_key_id_replacements.push_back(id_replacements);
        count++;
    }

    // If we have a one-to-one relationship with a 'default create' property
    // then create the other side of the relationship as a child.
    std::size_t instance_count{0};
    for (const auto& instance : working_fields.m_one_to_one) {
        for (const auto& [type, name] : instance) {
            auto child_dict = create_child(
                type, ids[instance_count], crud_request.get_user_context());
            content->get_sequence()[instance_count]->set_map_item(
                name, std::move(child_dict));
        }
        instance_count++;
    }

    const auto current_time = m_time_provider->get_current_time();

    ModelCreationContext create_context(item_template->get_runtime_type());
    create_context.m_creation_time.update_value(current_time);
    create_context.m_last_modified_time.update_value(current_time);

    if (item_template->has_owner()) {
        create_context.add_user(crud_request.get_user_context().m_id);
    }

    Dictionary create_context_dict;
    create_context.serialize(create_context_dict);

    assert(ids.size() == content->get_sequence().size());

    std::vector<KeyValuePair> string_set_kv_pairs;
    std::vector<KeyValuePair> set_add_kv_pairs;
    prepare_create_keys(
        string_set_kv_pairs, set_add_kv_pairs, ids, working_fields, *content,
        create_context_dict, item_template->get_primary_key_name());

    const auto response = m_client->make_request(
        {KeyValueStoreRequestMethod::STRING_SET, string_set_kv_pairs,
         m_config.m_endpoint});
    error_check("STRING_SET", response.get());

    const auto set_response = m_client->make_request(
        {KeyValueStoreRequestMethod::SET_ADD, set_add_kv_pairs,
         m_config.m_endpoint});
    error_check("SET_ADD", set_response.get());

    if (crud_request.get_query().is_attribute_output_format()) {
        get_adapter(CrudAttributes::Format::JSON)
            ->dict_to_string(*content, crud_response.attributes().buffer());
    }
    else if (crud_request.get_query().is_dict_output_format()) {
        crud_response.set_dict(std::move(content));
    }
    else if (crud_request.get_query().is_item_output_format()) {
        get_adapter(CrudAttributes::Format::JSON)
            ->from_dict(*content, crud_response.items());
    }

    if (record_modified_attrs) {
        for (const auto& item : content->get_sequence()) {
            Map flat_attrs;
            item->flatten(flat_attrs);
            crud_response.modified_attrs().push_back(flat_attrs);
        }
    }

    crud_response.ids() = ids;
}

void KeyValueCrudClient::update(
    const CrudRequest& crud_request,
    CrudResponse& crud_response,
    bool record_modified_attrs) const
{
    std::vector<std::string> ids;
    if (crud_request.has_items()) {
        for (const auto& item : crud_request.items()) {
            ids.push_back(item->get_primary_key());
        }
    }
    else {
        for (const auto& id : crud_request.get_ids()) {
            ids.push_back(id.get_primary_key());
        }
    }

    std::vector<std::string> string_get_keys;
    for (const auto& id : ids) {
        string_get_keys.push_back(get_item_key(id));
    }

    const auto response = m_client->make_request(
        {KeyValueStoreRequestMethod::STRING_GET, string_get_keys,
         m_config.m_endpoint});
    error_check("GET", response.get());

    const auto any_false = std::any_of(
        response->found().begin(), response->found().end(),
        [](bool v) { return !v; });
    if (any_false) {
        throw std::runtime_error("Attempted to update a non-existing resource");
    }

    VecModelPtr db_items;
    get_adapter(CrudAttributes::Format::JSON)
        ->from_string(response->items(), db_items);

    const auto current_time = m_time_provider->get_current_time();
    ModelUpdateContext update_context(m_adapters->get_type());
    update_context.m_last_modified_time.update_value(current_time);
    Dictionary update_context_dict;
    update_context.serialize(update_context_dict);

    std::size_t count = 0;
    if (crud_request.has_items()) {
        for (auto& db_item : db_items) {
            Dictionary modified_dict;
            crud_request.items()[count]->serialize(
                modified_dict, Serializeable::Format::MODIFIED);

            modified_dict.merge(update_context_dict);

            db_item->deserialize(modified_dict);
            count++;
        }
    }

    auto updated_content = std::make_unique<Dictionary>();
    auto adapter         = get_adapter(CrudAttributes::Format::JSON);

    adapter->to_dict(db_items, *updated_content);

    std::vector<KeyValuePair> string_set_kv_pairs;
    if (updated_content->get_type() == Dictionary::Type::SEQUENCE) {
        count = 0;
        assert(
            string_get_keys.size() == updated_content->get_sequence().size());
        for (const auto& dict_item : updated_content->get_sequence()) {
            std::string content;
            adapter->dict_to_string(*dict_item, content);
            string_set_kv_pairs.push_back({string_get_keys[count], content});
            count++;
        }
    }
    else {
        assert(string_get_keys.size() == 1);
        std::string content;
        adapter->dict_to_string(*updated_content, content);
        string_set_kv_pairs.push_back({string_get_keys[0], content});
    }

    auto set_response = m_client->make_request(
        {KeyValueStoreRequestMethod::STRING_SET, string_set_kv_pairs,
         m_config.m_endpoint});
    error_check("STRING_SET", set_response.get());

    if (crud_request.get_query().is_attribute_output_format()) {
        get_adapter(CrudAttributes::Format::JSON)
            ->dict_to_string(
                *updated_content, crud_response.attributes().buffer());
    }
    else if (crud_request.get_query().is_dict_output_format()) {
        crud_response.set_dict(std::move(updated_content));
    }
    else if (crud_request.get_query().is_item_output_format()) {
        get_adapter(CrudAttributes::Format::JSON)
            ->from_dict(*updated_content, crud_response.items());
    }

    if (record_modified_attrs) {
        for (const auto& item : updated_content->get_sequence()) {
            Map flat_attrs;
            item->flatten(flat_attrs);
            crud_response.modified_attrs().push_back(flat_attrs);
        }
    }
    crud_response.ids() = ids;
}

void KeyValueCrudClient::update_foreign_proxy_keys(
    const std::string& id,
    const VecKeyValuePair& fields,
    std::vector<std::string>& foreign_key_proxy_keys) const
{
    for (const auto& [type, name] : fields) {
        const auto key = m_config.m_prefix + ":" + m_adapters->get_type() + ":"
                         + id + ":" + type + "s";
        foreign_key_proxy_keys.push_back(key);
    }
}

bool KeyValueCrudClient::prepare_query_keys_with_id(
    std::vector<std::string>& string_get_keys,
    std::vector<std::string>& foreign_key_proxy_keys,
    const VecKeyValuePair& fields,
    const CrudQuery& query) const
{
    for (const auto& id : query.ids()) {
        std::string working_id = id.get_primary_key();
        if (id.has_primary_key()) {
            string_get_keys.push_back(get_item_key(working_id));
        }
        else if (id.has_name()) {
            std::string field_prefix;
            if (id.has_parent_primary_key()) {
                field_prefix += id.get_parent_primary_key() + "::";
            }
            const auto id_request_key =
                get_field_key(field_prefix + "name", id.get_name());
            const auto response = m_client->make_request(
                {KeyValueStoreRequestMethod::STRING_GET,
                 {id_request_key},
                 m_config.m_endpoint});
            error_check("STRING_GET", response.get());
            if (response->items().empty() || response->items()[0].empty()) {
                return false;
            }
            working_id = response->items()[0];
            string_get_keys.push_back(get_item_key(working_id));
        }
        update_foreign_proxy_keys(working_id, fields, foreign_key_proxy_keys);
    }
    return true;
}

bool KeyValueCrudClient::prepare_query_keys_with_filter(
    std::vector<std::string>& string_get_keys,
    std::vector<std::string>& foreign_key_proxy_keys,
    const VecKeyValuePair& fields,
    const CrudQuery& query) const
{
    const auto id_request_key = get_field_key(
        query.get_filter().data().begin()->first,
        query.get_filter().data().begin()->second);

    const auto response = m_client->make_request(
        {KeyValueStoreRequestMethod::STRING_GET,
         {id_request_key},
         m_config.m_endpoint});
    error_check("STRING_GET", response.get());
    const std::string working_id = response->items()[0];
    if (response->items().empty() || working_id.empty()) {
        return false;
    }
    string_get_keys.push_back(get_item_key(working_id));
    update_foreign_proxy_keys(working_id, fields, foreign_key_proxy_keys);
    return true;
}

void KeyValueCrudClient::prepare_query_keys_empty(
    std::vector<std::string>& string_get_keys,
    std::vector<std::string>& foreign_key_proxy_keys,
    const VecKeyValuePair& fields) const
{
    const auto response = m_client->make_request(
        {KeyValueStoreRequestMethod::SET_LIST,
         {get_set_key()},
         m_config.m_endpoint});
    error_check("SET_LIST", response.get());

    if (!response->ids().empty()) {
        for (const auto& id : response->ids()[0]) {
            string_get_keys.push_back(get_item_key(id));
            update_foreign_proxy_keys(id, fields, foreign_key_proxy_keys);
        }
    }
}

void KeyValueCrudClient::update_proxy_dicts(
    Dictionary& foreign_key_dict,
    const VecKeyValuePair& fields,
    const std::vector<std::string>& string_get_keys,
    const std::vector<std::string>& foreign_key_proxy_keys) const
{
    const auto proxy_response = m_client->make_request(
        {KeyValueStoreRequestMethod::SET_LIST, foreign_key_proxy_keys,
         m_config.m_endpoint});
    error_check("SET LIST", proxy_response.get());

    std::vector<std::string> proxy_value_keys;
    std::vector<std::size_t> proxy_value_offsets;
    for (std::size_t idx = 0; idx < string_get_keys.size(); idx++) {
        for (std::size_t jdx = 0; jdx < fields.size(); jdx++) {
            if (proxy_response->ids().empty()) {
                proxy_value_offsets.push_back(0);
            }
            else {
                const auto offset = idx * fields.size() + jdx;
                const auto type   = fields[jdx].first;
                assert(offset < proxy_response->ids().size());
                std::size_t count{0};
                for (const auto& id : proxy_response->ids()[offset]) {
                    const auto key = m_config.m_prefix + ":" + type + ":" + id;
                    proxy_value_keys.push_back(key);
                    count++;
                }
                proxy_value_offsets.push_back(count);
            }
        }
    }

    const auto proxy_data_response = m_client->make_request(
        {KeyValueStoreRequestMethod::STRING_GET, proxy_value_keys,
         m_config.m_endpoint});
    error_check("GET", proxy_data_response.get());

    std::size_t count = 0;
    auto adapter      = get_adapter(CrudAttributes::Format::JSON);
    for (std::size_t idx = 0; idx < string_get_keys.size(); idx++) {
        auto item_dict = std::make_unique<Dictionary>();
        for (std::size_t jdx = 0; jdx < fields.size(); jdx++) {
            const auto offset = idx * fields.size() + jdx;
            const auto name   = fields[jdx].second;

            auto item_field_seq_dict =
                std::make_unique<Dictionary>(Dictionary::Type::SEQUENCE);
            for (std::size_t kdx = 0; kdx < proxy_value_offsets[offset];
                 kdx++) {
                const auto db_value   = proxy_data_response->items()[count];
                auto field_entry_dict = std::make_unique<Dictionary>();
                adapter->dict_from_string(db_value, *field_entry_dict);
                item_field_seq_dict->add_sequence_item(
                    std::move(field_entry_dict));
                count++;
            }

            item_dict->set_map_item(name, std::move(item_field_seq_dict));
        }
        foreign_key_dict.add_sequence_item(std::move(item_dict));
    }
}

void on_empty_read(
    const CrudQuery& query, CrudResponse& crud_response, bool expect_single)
{
    if (query.is_dict_output_format()) {
        auto response_dict = std::make_unique<Dictionary>();
        if (!expect_single) {
            response_dict->set_type(Dictionary::Type::SEQUENCE);
        }
        crud_response.set_dict(std::move(response_dict));
    }
}

void KeyValueCrudClient::read(
    const CrudRequest& request, CrudResponse& crud_response) const
{
    std::vector<std::string> string_get_keys;
    std::vector<std::string> foreign_key_proxy_keys;

    const auto expect_single =
        (request.get_query().is_id() && request.get_query().ids().size() == 1)
        || (request.get_query().is_filter()
            && request.get_query().get_format() == CrudQuery::Format::GET);

    auto template_item = m_adapters->get_model_factory()->create();
    VecKeyValuePair foreign_key_proxies;
    template_item->get_foreign_key_proxy_fields(foreign_key_proxies);

    if (request.get_query().is_id()) {
        if (!prepare_query_keys_with_id(
                string_get_keys, foreign_key_proxy_keys, foreign_key_proxies,
                request.get_query())) {
            on_empty_read(request.get_query(), crud_response, expect_single);
            return;
        }
    }
    else {
        if (request.get_query().get_filter().empty()) {
            prepare_query_keys_empty(
                string_get_keys, foreign_key_proxy_keys, foreign_key_proxies);
        }
        else {
            if (!prepare_query_keys_with_filter(
                    string_get_keys, foreign_key_proxy_keys,
                    foreign_key_proxies, request.get_query())) {
                on_empty_read(
                    request.get_query(), crud_response, expect_single);
                return;
            }
        }
    }

    const auto response = m_client->make_request(
        {KeyValueStoreRequestMethod::STRING_GET, string_get_keys,
         m_config.m_endpoint});
    error_check("GET", response.get());
    if (string_get_keys.empty()) {
        on_empty_read(request.get_query(), crud_response, expect_single);
        return;
    }

    const auto any_empty = std::any_of(
        response->items().begin(), response->items().end(),
        [](const std::string& entry) { return entry.empty(); });
    if (any_empty) {
        on_empty_read(request.get_query(), crud_response, expect_single);
        return;
    }

    auto adapter = get_adapter(CrudAttributes::Format::JSON);

    Dictionary foreign_key_dict(Dictionary::Type::SEQUENCE);
    if (!foreign_key_proxy_keys.empty()) {
        update_proxy_dicts(
            foreign_key_dict, foreign_key_proxies, string_get_keys,
            foreign_key_proxy_keys);
    }

    auto response_dict = std::make_unique<Dictionary>();

    adapter->from_string(response->items(), *response_dict, !expect_single);

    if (!foreign_key_dict.is_empty()) {
        assert(!foreign_key_dict.get_sequence().empty());
        if (response_dict->get_type() == Dictionary::Type::SEQUENCE) {
            for (std::size_t idx = 0;
                 idx < response_dict->get_sequence().size(); idx++) {
                response_dict->get_sequence()[idx]->merge(
                    *foreign_key_dict.get_sequence()[idx]);
            }
        }
        else {
            response_dict->merge(*foreign_key_dict.get_sequence()[0]);
        }
    }

    auto item_template = m_adapters->get_model_factory()->create();
    response_dict->get_scalars(
        item_template->get_primary_key_name(), crud_response.ids());

    if (request.get_query().is_attribute_output_format()) {
        adapter->dict_to_string(
            *response_dict, crud_response.attributes().buffer());
    }
    else if (request.get_query().is_item_output_format()) {
        adapter->from_dict(*response_dict, crud_response.items());
    }
    else if (request.get_query().is_dict_output_format()) {
        crud_response.set_dict(std::move(response_dict));
    }
}

void KeyValueCrudClient::remove(const VecCrudIdentifier& ids) const
{
    std::vector<std::string> string_remove_keys;
    std::vector<KeyValuePair> set_remove_keys;
    for (const auto& id : ids) {
        if (id.has_primary_key()) {
            string_remove_keys.push_back(get_item_key(id.get_primary_key()));
            set_remove_keys.push_back({get_set_key(), id.get_primary_key()});
        }
    }

    const auto string_response = m_client->make_request(
        {KeyValueStoreRequestMethod::STRING_REMOVE, string_remove_keys,
         m_config.m_endpoint});
    error_check("STRING_REMOVE", string_response.get());

    const auto set_response = m_client->make_request(
        {KeyValueStoreRequestMethod::SET_REMOVE, set_remove_keys,
         m_config.m_endpoint});
    error_check("SET_REMOVE", string_response.get());
}

void KeyValueCrudClient::identify(
    const VecCrudIdentifier& ids, CrudResponse& response) const
{
    (void)ids;
    (void)response;
}

void KeyValueCrudClient::lock(
    const CrudIdentifier& id, CrudLockType lock_type) const
{
    const auto response = m_client->make_request(
        {KeyValueStoreRequestMethod::STRING_SET,
         {KeyValuePair(get_lock_key(id.get_primary_key(), lock_type), "1")},
         m_config.m_endpoint});
    error_check("STRING_SET", response.get());
}

void KeyValueCrudClient::unlock(
    const CrudIdentifier& id, CrudLockType lock_type) const
{
    const auto response = m_client->make_request(
        {KeyValueStoreRequestMethod::STRING_REMOVE,
         {get_lock_key(id.get_primary_key(), lock_type)},
         m_config.m_endpoint});
    error_check("STRING_REMOVE", response.get());
}

bool KeyValueCrudClient::is_locked(
    const CrudIdentifier& id, CrudLockType lock_type) const
{
    const auto response = m_client->make_request(
        {KeyValueStoreRequestMethod::STRING_EXISTS,
         {get_lock_key(id.get_primary_key(), lock_type)},
         m_config.m_endpoint});

    error_check("STRING_EXISTS", response.get());
    return response->found()[0];
}

std::string KeyValueCrudClient::get_lock_key(
    const std::string& id, CrudLockType lock_type) const
{
    std::string lock_str = lock_type == CrudLockType::READ ? "r" : "w";
    return get_prefix() + "_lock" + lock_str + ":" + id;
}

std::string KeyValueCrudClient::get_item_key(const std::string& id) const
{
    return get_prefix() + ":" + id;
}

std::string KeyValueCrudClient::get_field_key(
    const std::string& field, const std::string& value) const
{
    return get_prefix() + "_" + field + ":" + value;
}

void KeyValueCrudClient::get_item_keys(
    const std::vector<std::string>& ids, std::vector<std::string>& keys) const
{
    for (const auto& id : ids) {
        keys.push_back(get_prefix() + ":" + id);
    }
}

std::string KeyValueCrudClient::get_prefix() const
{
    return m_config.m_prefix + ":" + m_adapters->get_type();
}

std::string KeyValueCrudClient::get_set_key() const
{
    return get_prefix() + "s";
}

void KeyValueCrudClient::error_check(
    const std::string& identifier, BaseResponse* response) const
{
    if (!response->ok()) {
        const std::string msg = "Error in kv_store " + identifier + ": "
                                + response->get_base_error().to_string();
        LOG_ERROR(msg);
        throw RequestException<CrudRequestError>({CrudErrorCode::ERROR, msg});
    }
}

}  // namespace hestia