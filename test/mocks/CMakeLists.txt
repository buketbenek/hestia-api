add_subdirectory(motr)
add_subdirectory(phobos)

add_module(
    MODULE_NAME mocks 
    HEADERS
        MockS3Server.h
        MockWebService.h
        MockWebView.h
        MockModel.h 
        MockCrudService.h
        MockCrudWebApp.h
        MockCrudView.h
        MockHestiaClient.h
    SOURCES
        MockS3Server.cc
        MockCrudView.cc
        MockModel.cc
        MockCrudView.cc
        MockCrudService.cc
        MockCrudWebApp.cc
        MockHestiaClient.cc
    INTERNAL_DEPENDENCIES 
        protocol
        web
        app
)

add_dependencies(hestia_mocks hestia_mock_phobos_plugin hestia_mock_motr_plugin)