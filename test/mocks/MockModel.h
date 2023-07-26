#pragma once

#include "Model.h"
#include "RelationshipField.h"
#include "StringAdapter.h"

namespace hestia::mock {
class MockModel : public Model {
  public:
    MockModel();

    MockModel(const std::string& id);

    MockModel(const MockModel& other);

    MockModel& operator=(const MockModel& other);

    static std::string get_type();

    static std::unique_ptr<ModelFactory> create_factory();

    static AdapterCollection::Ptr create_adapters();

    void set_parent_id(const std::string& id) { m_parent.set_id(id); }

    void init();

    static constexpr char s_name[]{"mock_model"};
    StringField m_my_field{"my_field", "default_field"};
    NamedForeignKeyField m_parent{"parent", "mock_parent_model"};
};

class MockParentModel : public Model {
  public:
    MockParentModel();

    MockParentModel(const std::string& id);

    MockParentModel(const MockParentModel& other);

    MockParentModel& operator=(const MockParentModel& other);

    static std::string get_type();

    static std::unique_ptr<ModelFactory> create_factory();

    static AdapterCollection::Ptr create_adapters();

    const std::vector<MockModel>& get_models() const
    {
        return m_models.models();
    }

    void init();

    static constexpr char s_name[]{"mock_parent_model"};
    StringField m_my_field{"my_field", "default_field"};
    ForeignKeyProxyField<MockModel> m_models{"models"};
};
}  // namespace hestia::mock