#include "S3Config.h"

#include <cstdlib>

namespace hestia {

S3Config::S3Config() : SerializeableWithFields("s3_object_store_client_config")
{
    init();
}

void S3Config::init()
{
    register_scalar_field(&m_metadataprefix);
    register_scalar_field(&m_user_agent);
    register_scalar_field(&m_default_host);
    register_scalar_field(&m_default_bucket_name);
    register_scalar_field(&m_uri_style);
    register_scalar_field(&m_s3_access_key_id);
    register_scalar_field(&m_s3_secret_access_key_var);
}

const std::string& S3Config::get_user_agent() const
{
    return m_user_agent.get_value();
}

const std::string& S3Config::get_default_host() const
{
    return m_default_host.get_value();
}

const std::string& S3Config::get_access_key_id() const
{
    return m_s3_access_key_id.get_value();
}

std::string S3Config::get_secret_access_key() const
{
    const auto env_variable = m_s3_secret_access_key_var.get_value();
    if (env_variable.empty()) {
        return {};
    }

    if (auto config_path_env = std::getenv(env_variable.c_str());
        config_path_env != nullptr) {
        return config_path_env;
    }
    return {};
}

}  // namespace hestia