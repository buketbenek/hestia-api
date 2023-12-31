#include <catch2/catch_all.hpp>

#include "HestiaS3WebApp.h"

#include "ObjectStoreBackend.h"

#include "DistributedHsmService.h"
#include "HsmService.h"
#include "S3Request.h"
#include "StorageTier.h"
#include "TypedCrudRequest.h"
#include "UserService.h"

#include "InMemoryHsmObjectStoreClient.h"
#include "InMemoryKeyValueStoreClient.h"
#include "RequestContext.h"

#include "DistributedHsmServiceTestWrapper.h"

#include "Logger.h"
#include <iostream>

class WebAppTestFixture {
  public:
    WebAppTestFixture()
    {
        m_fixture = std::make_unique<DistributedHsmServiceTestWrapper>();
        m_fixture->init("AKIAIOSFODNN7EXAMPLE", "my_admin_password", 5);

        hestia::HestiaS3WebAppConfig app_config;
        m_web_app = std::make_unique<hestia::HestiaS3WebApp>(
            app_config, m_fixture->m_dist_hsm_service.get(),
            m_fixture->m_user_service.get());
    }

    void add_s3_headers(hestia::HttpRequest& req)
    {
        hestia::S3Request s3_request(req);
        s3_request.m_timestamp.m_value = "20130524";
        s3_request.m_region            = "us-east-1";
        s3_request.set_user_id("AKIAIOSFODNN7EXAMPLE");
        s3_request.set_user_secret_key(m_fixture->m_token_generator->m_token);
        s3_request.m_signed_headers = {
            "host", "range", "x-amz-content-sha256", "x-amz-date"};

        s3_request.populate_headers("", req.get_header());
        s3_request.populate_authorization_headers(
            hestia::S3Request::PayloadSignatureType::UNSIGNED, req);
    }

    hestia::HttpResponse* make_request(
        const std::string& path,
        hestia::HttpRequest::Method method,
        const std::string& data = {})
    {
        m_working_context = std::make_unique<hestia::RequestContext>();
        m_working_context->set_request(hestia::HttpRequest{path, method});
        add_s3_headers(m_working_context->get_writeable_request());

        if (method == hestia::HttpRequest::Method::PUT && !data.empty()) {
            m_working_context->get_writeable_request().get_header().set_item(
                "Content-Length", std::to_string(data.size()));
        }

        /*
        if (method == hestia::HttpRequest::Method::GET && !data.empty())
        {
            std::vector<char> working_buffer;
            m_working_context->set_output_chunk_handler(
                [&working_buffer](
                    const hestia::ReadableBufferView& buffer, bool finished) {
                    (void)finished;
                    for (std::size_t idx = 0; idx < buffer.length(); idx++) {
                        working_buffer.push_back(buffer.data()[idx]);
                    }
                    return buffer.length();
                });

            m_working_context->set_output_complete_handler(
                [&working_buffer, &data](const hestia::HttpResponse* response) {
                    REQUIRE(response->code() == 200);
                    data =
                        std::string(working_buffer.begin(),
        working_buffer.end());
                });

            m_web_app->on_event(
                m_working_context.get(), hestia::HttpEvent::HEADERS);
            if (m_working_context->get_response()->get_completion_status()
                != hestia::HttpResponse::CompletionStatus::FINISHED) {
                m_web_app->on_event(
                    m_working_context.get(), hestia::HttpEvent::EOM);
            }

            auto response = m_working_context->get_response();
            REQUIRE(response->code() == 200);

            m_working_context->flush_stream();
        }
        */
        if (method == hestia::HttpRequest::Method::PUT && !data.empty()) {
            m_web_app->on_event(
                m_working_context.get(), hestia::HttpEvent::HEADERS);

            if (m_working_context->get_response()->get_completion_status()
                != hestia::HttpResponse::CompletionStatus::FINISHED) {
                std::size_t chunk_size = 10;
                std::vector<char> data_chars(data.begin(), data.end());
                std::size_t cursor = 0;
                while (cursor < data.size()) {
                    auto chunk_end = cursor + chunk_size;
                    if (chunk_end >= data.size()) {
                        chunk_end = data.size();
                    }
                    if (chunk_end == cursor) {
                        break;
                    }

                    std::vector<char> chunk(
                        data_chars.begin() + cursor,
                        data_chars.begin() + chunk_end);
                    hestia::ReadableBufferView read_buffer(chunk);

                    REQUIRE(
                        m_working_context->write_to_stream(read_buffer).ok());
                    cursor = chunk_end;
                }
                (void)m_working_context->clear_stream();

                m_web_app->on_event(
                    m_working_context.get(), hestia::HttpEvent::EOM);
            }
        }
        else {
            m_web_app->on_event(
                m_working_context.get(), hestia::HttpEvent::HEADERS);

            if (m_working_context->get_response()->get_completion_status()
                != hestia::HttpResponse::CompletionStatus::FINISHED) {
                m_web_app->on_event(
                    m_working_context.get(), hestia::HttpEvent::EOM);
            }
        }
        return m_working_context->get_response();
    }

    std::unique_ptr<DistributedHsmServiceTestWrapper> m_fixture;
    std::unique_ptr<hestia::HestiaS3WebApp> m_web_app;
    std::unique_ptr<hestia::RequestContext> m_working_context;
};

TEST_CASE_METHOD(WebAppTestFixture, "Test s3 web app", "[s3]")
{
    auto response = make_request("/", hestia::HttpRequest::Method::GET);
    REQUIRE(response->code() == 200);
    REQUIRE_FALSE(response->body().empty());
    REQUIRE(
        response->header().get_content_length()
        == std::to_string(response->body().size()));

    response = make_request("/mybucket", hestia::HttpRequest::Method::GET);
    REQUIRE(response->code() == 404);

    response = make_request("/mybucket", hestia::HttpRequest::Method::PUT);
    REQUIRE(response->code() == 201);

    response = make_request("/mybucket", hestia::HttpRequest::Method::GET);
    REQUIRE(response->code() == 200);
    REQUIRE_FALSE(response->body().empty());
    REQUIRE(
        response->header().get_content_length()
        == std::to_string(response->body().size()));

    response =
        make_request("/mybucket/myobject", hestia::HttpRequest::Method::GET);
    REQUIRE(response->code() == 404);

    const std::string obj_data = "The quick brown fox jumps over the lazy dog.";
    response                   = make_request(
        "/mybucket/myobject", hestia::HttpRequest::Method::PUT, obj_data);
    REQUIRE(response->code() == 200);

    response =
        make_request("/mybucket/myobject", hestia::HttpRequest::Method::HEAD);
    REQUIRE(response->code() == 200);

    /*
    std::string returned_data = "0";
    response =
        make_request("/mybucket/myobject", hestia::HttpRequest::Method::GET,
    returned_data); REQUIRE(response->code() == 200); REQUIRE(returned_data ==
    obj_data);
    */
}