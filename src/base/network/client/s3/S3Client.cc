#include "S3Client.h"

#include "HttpRequest.h"
#include "XmlAttribute.h"
#include "XmlDocument.h"
#include "XmlElement.h"

namespace hestia {

S3Client::S3Client(HttpClient* http_client) : m_http_client(http_client) {}

S3Client::~S3Client() {}

S3Status S3Client::create_bucket(
    const S3Bucket& bucket, const S3Request& s3_request) const
{
    const auto status = bucket.validate_name(
        s3_request.m_tracking_id, s3_request.get_user_context());
    if (!status.is_ok()) {
        return status;
    }

    const auto path = s3_request.get_resource_path(bucket.name());
    HttpRequest http_request(path, HttpRequest::Method::PUT);
    s3_request.populate_headers(bucket.name(), http_request.get_header());

    // Prepare body
    auto create_element = XmlElement::create("CreateBucketConfiguration");
    S3Request::add_aws_namespace(*create_element);
    bucket.add_location_constraint(*create_element);
    http_request.body() = XmlDocument::to_string(*create_element);

    // Post-body header updates
    s3_request.populate_authorization_headers(
        S3Request::PayloadSignatureType::SIGNED, http_request);

    return do_request(http_request)->m_status;
}

S3Status S3Client::head_bucket(
    const S3Bucket& bucket, const S3Request& s3_request) const
{
    const auto path = s3_request.get_resource_path(bucket.name());

    HttpRequest http_request(path, HttpRequest::Method::HEAD);
    s3_request.populate_headers(bucket.name(), http_request.get_header());
    s3_request.populate_authorization_headers(
        S3Request::PayloadSignatureType::SIGNED, http_request);

    return do_request(http_request)->m_status;
}

S3Status S3Client::delete_bucket(
    const S3Bucket& bucket, const S3Request& s3_request) const
{
    const auto path = s3_request.get_resource_path(bucket.name());

    HttpRequest http_request(path, HttpRequest::Method::DELETE);
    s3_request.populate_headers(bucket.name(), http_request.get_header());
    s3_request.populate_authorization_headers(
        S3Request::PayloadSignatureType::SIGNED, http_request);

    return do_request(http_request)->m_status;
}

S3ListBucketResponse::Ptr S3Client::list_buckets(
    const S3Request& s3_request) const
{
    const auto path = s3_request.m_endpoint;

    HttpRequest http_request(path, HttpRequest::Method::GET);
    s3_request.populate_headers({}, http_request.get_header());
    s3_request.populate_authorization_headers(
        S3Request::PayloadSignatureType::SIGNED, http_request);

    return std::make_unique<S3ListBucketResponse>(*do_request(http_request));
}

S3Status S3Client::put_object(
    const S3Object& object,
    const S3Bucket& bucket,
    const S3Request& s3_request,
    Stream* stream)
{
    const auto path = s3_request.get_resource_path(bucket.name(), object.m_key);

    HttpRequest http_request(path, HttpRequest::Method::PUT);
    s3_request.populate_headers(
        bucket.name(), http_request.get_header(), object.m_content_mimetype);
    s3_request.populate_authorization_headers(
        S3Request::PayloadSignatureType::UNSIGNED, http_request);

    const auto response = std::make_unique<S3Response>(
        m_http_client->make_request(http_request, stream));
    return response->m_status;
}

S3Status S3Client::delete_object(
    const S3Object& object,
    const S3Bucket& bucket,
    const S3Request& s3_request) const
{
    const auto path = s3_request.get_resource_path(bucket.name(), object.m_key);

    HttpRequest http_request(path, HttpRequest::Method::DELETE);
    s3_request.populate_headers(bucket.name(), http_request.get_header());
    s3_request.populate_authorization_headers(
        S3Request::PayloadSignatureType::SIGNED, http_request);

    return do_request(http_request)->m_status;
}

S3Status S3Client::get_object(
    const S3Object& object,
    const S3Bucket& bucket,
    const S3Request& s3_request,
    Stream* stream)
{
    const auto path = s3_request.get_resource_path(bucket.name(), object.m_key);

    HttpRequest http_request(path, HttpRequest::Method::GET);
    s3_request.populate_headers(bucket.name(), http_request.get_header());
    s3_request.populate_authorization_headers(
        S3Request::PayloadSignatureType::UNSIGNED, http_request);

    auto response = std::make_unique<S3Response>(
        m_http_client->make_request(http_request, stream));
    return response->m_status;
}

S3ListObjectsResponse::Ptr S3Client::list_objects(
    const S3Bucket& bucket, const S3ListObjectsRequest& request)
{
    const auto path = request.m_s3_request.get_resource_path(bucket.name());

    HttpRequest http_request(path, HttpRequest::Method::GET);

    Map query;
    request.build_query(query);
    http_request.set_queries(query);

    request.m_s3_request.populate_headers(
        bucket.name(), http_request.get_header());
    request.m_s3_request.populate_authorization_headers(
        S3Request::PayloadSignatureType::UNSIGNED, http_request);

    return std::make_unique<S3ListObjectsResponse>(
        *do_request(http_request), request.m_is_v2_type);
}

S3Response::Ptr S3Client::do_request(const HttpRequest& http_request) const
{
    return std::make_unique<S3Response>(
        m_http_client->make_request(http_request));
}

}  // namespace hestia