#include <catch2/catch_all.hpp>

#include "hestia.h"
#include "hestia_private.h"

#include "JsonUtils.h"
#include "MockHestiaClient.h"
#include "TestClientConfigs.h"

#include <iostream>

class HestiaCApiTestFixture {
  public:
    HestiaCApiTestFixture()
    {
        hestia::Dictionary extra_config;
        hestia::TestClientConfigs::get_hsm_memory_client_config(extra_config);

        auto rc = hestia_initialize(
            nullptr, nullptr,
            hestia::JsonDocument(extra_config).to_string().c_str());
        REQUIRE(rc == 0);

        auto mock_client = std::make_unique<hestia::mock::MockHestiaClient>();
        m_client         = mock_client.get();
    }

    ~HestiaCApiTestFixture() { hestia_finish(); }

    void do_create(std::string& id)
    {
        char* output{nullptr};
        int len_output{0};

        LOG_INFO("starting create");

        int rc{0};
        if (id.empty()) {
            rc = hestia_create(
                HESTIA_OBJECT, HESTIA_IO_NONE, HESTIA_ID_NONE, nullptr, 0,
                HESTIA_IO_IDS, &output, &len_output);
        }
        else {
            rc = hestia_create(
                HESTIA_OBJECT, HESTIA_IO_IDS, HESTIA_ID, id.c_str(), id.size(),
                HESTIA_IO_IDS, &output, &len_output);
        }

        LOG_INFO("Finished create");

        REQUIRE(rc == 0);
        REQUIRE(len_output > 0);
        REQUIRE(output != nullptr);

        id = std::string(output, len_output);
        delete[] output;
    }

    void do_read(std::string& id)
    {
        char* output{nullptr};
        int len_output{0};
        int total_count{0};

        const auto rc = hestia_read(
            HESTIA_OBJECT, HESTIA_QUERY_NONE, HESTIA_ID_NONE, 0, 10, nullptr, 0,
            HESTIA_IO_IDS, &output, &len_output, &total_count);
        REQUIRE(rc == 0);
        REQUIRE(len_output > 0);
        REQUIRE(output != nullptr);

        id = std::string(output, len_output);
        delete[] output;
    }

    void do_put(const std::string& id, const std::string& content)
    {
        char* activity_id{nullptr};
        int len_activity_id{0};

        const auto rc = hestia_data_put(
            id.c_str(), content.data(), content.length(), 0, 0, &activity_id,
            &len_activity_id);
        REQUIRE(rc == 0);

        std::string activity_id_str = std::string(activity_id, len_activity_id);
        delete[] activity_id;
    }

    void do_get(const std::string& id, std::string& content)
    {
        char* activity_id{nullptr};
        int len_activity_id{0};

        std::vector<char> buffer(content.length());
        std::size_t length = content.length();

        const auto rc = hestia_data_get(
            id.c_str(), buffer.data(), &length, 0, 0, &activity_id,
            &len_activity_id);
        REQUIRE(rc == 0);

        std::string activity_id_str = std::string(activity_id, len_activity_id);
        hestia_free_output(&activity_id);

        content = std::string(buffer.begin(), buffer.begin() + length);
    }

    hestia::mock::MockHestiaClient* m_client{nullptr};
};

TEST_CASE_METHOD(HestiaCApiTestFixture, "Test Hestia C API", "[hestia]")
{
    std::string id{"mock_id_0"};
    do_create(id);
    REQUIRE(id == "mock_id_0");

    std::string read_id;
    do_read(read_id);
    REQUIRE(read_id == id);

    std::string content("The quick brown fox jumps over the lazy dog");
    do_put(id, content);

    std::string returned_content(content.length(), '0');
    do_get(id, returned_content);
    REQUIRE(content == returned_content);
}