#include "HsmObjectStoreClientPlugin.h"

#include <ostk/Logger.h>

#include <exception>

HsmObjectStoreClientPlugin::HsmObjectStoreClientPlugin(
    const ostk::PluginHandle* plugin_handle, HsmObjectStoreClient* client) :
    PluginResource(pluginHandle), m_client(client)
{
}

HsmObjectStoreClientPlugin::~HsmObjectStoreClientPlugin()
{
    mPluginHandle->destroyResource(this);
}

HsmObjectStoreClient* HsmObjectStoreClientPlugin::get_client() const
{
    return m_client;
}

HsmObjectStorePluginHandle::HsmObjectStorePluginHandle(
    void* raw_handle, const std::string& name) :
    ostk::PluginHandle(rawHandle, name)
{
}

void HsmObjectStorePluginHandle::destroy_resource(
    ostk::PluginResource* resource) const
{
    LOG_INFO("Destroying resource from: " << mName);
    using func_t      = void(HsmObjectStoreClient*);
    auto destroy_func = reinterpret_cast<func_t*>(mDestroyFunc);
    if (!destroy_func) {
        throw std::runtime_error("Function to destory resource is invalid");
    }

    if (auto object_resource =
            dynamic_cast<HsmObjectStoreClientPlugin*>(resource)) {
        destroy_func(object_resource->getClient());
    }
    else {
        LOG_ERROR("Failed to cast to ObjectStoreClientPlugin");
    }
}

std::unique_ptr<ostk::PluginResource>
HsmObjectStorePluginHandle::load_resource_with_factory_func() const
{
    using func_t     = HsmObjectStoreClient*();
    auto create_func = reinterpret_cast<func_t*>(mCreateFunc);
    auto client      = create_func();
    if (!client) {
        LOG_ERROR("Got invalid client from factory func.");
    }

    auto client_plugin =
        std::make_unique<HsmObjectStoreClientPlugin>(this, client);
    return client_plugin;
}

HsmObjectStorePluginFactory::HsmObjectStorePluginFactory(
    const std::string& name) :
    ostk::PluginFactory(name)
{
}

std::unique_ptr<ostk::PluginHandle> HsmObjectStorePluginFactory::create_handle(
    void* rawHandle) const
{
    return std::make_unique<HsmObjectStorePluginHandle>(rawHandle, mName);
}