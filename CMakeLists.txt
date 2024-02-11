cmake_minimum_required(VERSION 3.20)

project(shinysocks
    DESCRIPTION "Socks Proxy server"
    HOMEPAGE_URL https://github.com/jgaa/shinysocks
    VERSION 1.3.1
    LANGUAGES CXX
)

add_definitions(-DSHINYSOCKS_VERSION=\"${CMAKE_PROJECT_VERSION}\")

include(cmake/3rdparty.cmake)

find_package(Threads REQUIRED)

if(NOT DEFINED USE_BOOST_VERSION)
    set(USE_BOOST_VERSION 1.69)
endif()

find_package(Boost REQUIRED COMPONENTS
    system
    program_options
    serialization
    context
    coroutine
    )

add_executable(${PROJECT_NAME} Manager.cpp Listener.cpp Proxy.cpp main.cpp shinysocks.h logging.h)
add_dependencies(${PROJECT_NAME} logfault)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        $<BUILD_INTERFACE:${Boost_LIBRARIES}>
        $<BUILD_INTERFACE:${stdc++fs}>
        $<BUILD_INTERFACE:Threads::Threads>
    )

install(TARGETS shinysocks RUNTIME DESTINATION bin)

