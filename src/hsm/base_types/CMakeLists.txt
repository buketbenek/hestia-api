include(AddModule)

add_module(
    MODULE_NAME hsm_base_types
    HEADERS
        HsmItem.h
        HsmObject.h
        HsmAction.h
        StorageTier.h
        TierExtents.h
        Dataset.h
        Namespace.h
        UserMetadata.h
        ObjectStoreBackend.h 
        HsmNode.h
    SOURCES
        HsmItem.cc
        HsmObject.cc
        HsmAction.cc
        StorageTier.cc 
        TierExtents.cc
        Dataset.cc
        Namespace.cc
        UserMetadata.cc
        ObjectStoreBackend.cc
        HsmNode.cc
    INTERNAL_DEPENDENCIES 
        common
        models
        storage
)