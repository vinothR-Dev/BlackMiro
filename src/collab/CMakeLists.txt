# Live collaboration: relay server (not available in the browser build, which can only join).
if(NOT EMSCRIPTEN)
    qt_add_library(collab STATIC
        RelayFrame.h
        RelayServer.h RelayServer.cpp
    )
    target_link_libraries(collab PUBLIC Qt6::Core Qt6::Network Qt6::WebSockets)
else()
    add_library(collab INTERFACE)
endif()
target_include_directories(collab INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT EMSCRIPTEN)
    target_include_directories(collab PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
endif()
