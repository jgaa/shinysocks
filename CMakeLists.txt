cmake_minimum_required(VERSION 3.20)

if(WIN32) 
    set (WINVER "0x0601" CACHE STRING "Value for _WIN32_WINNT")
    add_definitions(-D_WIN32_WINNT=${WINVER})
endif()

project(shinysocks
    DESCRIPTION "Socks Proxy server"
    HOMEPAGE_URL https://github.com/jgaa/shinysocks
    VERSION 1.4.0
    LANGUAGES CXX
)

option(USE_STATIC_BOOST "Link Boost statically" ON)

add_definitions(-DSHINYSOCKS_VERSION=\"${CMAKE_PROJECT_VERSION}\")

include(cmake/3rdparty.cmake)

find_package(Threads REQUIRED)

if(NOT DEFINED USE_BOOST_VERSION)
    set(USE_BOOST_VERSION 1.69)
endif()

if(USE_STATIC_BOOST)
    set(Boost_USE_STATIC_LIBS   ON)
endif()

find_package(Boost ${USE_BOOST_VERSION} REQUIRED COMPONENTS
    system
    program_options
    serialization
    coroutine
    context
    )
add_executable(${PROJECT_NAME} Manager.cpp Listener.cpp Proxy.cpp main.cpp shinysocks.h logging.h)
add_dependencies(${PROJECT_NAME} logfault)
include_directories(${Boost_INCLUDE_DIR})

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    CXX_STANDARD 17
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${Boost_LIBRARIES}
        $<BUILD_INTERFACE:Threads::Threads>
    )

install(TARGETS shinysocks RUNTIME DESTINATION bin)

