cmake_minimum_required(VERSION 3.10)
project(shinysocks)


set(Boost_USE_MULTITHREADED ON)

find_package(Boost REQUIRED COMPONENTS
    system
    program_options
    serialization
    filesystem
    date_time
    iostreams
    regex
    context
    coroutine
    chrono
    log
    thread
    )

if (WIN32)
    set(LIB_BOOST_PROGRAM_OPTIONS Boost::program_options)
    set(LIB_BOOST_SERIALIZATION Boost::serialization)
    set(LIB_BOOST_FILESYSTEM Boost::filesystem)
    set(LIB_BOOST_DATE_TIME Boost::date_time)
    set(LIB_BOOST_IOSTREAMS Boost::iostreams)
    set(LIB_BOOST_SYSTEM Boost::system)
    set(LIB_BOOST_REGEX Boost::regex)
    set(LIB_BOOST_CONTEXT Boost::context)
    set(LIB_BOOST_COROUTINE Boost::coroutine)
    set(LIB_BOOST_CHRONO Boost::chrono)
    set(LIB_BOOST_THREAD Boost::thread)
    set(LIB_BOOST_LOG Boost::log Boost::log_setup)
    set(BOOST_UNIT_TEST_FRAMEWORK Boost::unit_test_framework)
    add_definitions(-D_WIN32_WINNT=0x0600)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")
    # Msvc and possible some other Windows-compilers will link
    # to the correct libraries trough #pragma directives in boost headers.
    #SET(BOOST_UNIT_TEST_FRAMEWORK libboost_test_exec_monitor-vc140-mt-sgd-1_57)
else ()
    set(LIB_BOOST_PROGRAM_OPTIONS boost_program_options)
    set(LIB_BOOST_SERIALIZATION boost_serialization)
    set(LIB_BOOST_FILESYSTEM boost_filesystem)
    set(LIB_BOOST_DATE_TIME boost_date_time)
    set(LIB_BOOST_IOSTREAMS boost_iostreams)
    set(LIB_BOOST_SYSTEM boost_system)
    set(LIB_BOOST_REGEX boost_regex)
    set(LIB_BOOST_CONTEXT boost_context)
    set(LIB_BOOST_COROUTINE boost_coroutine)
    set(LIB_BOOST_CHRONO boost_chrono)
    set(LIB_BOOST_THREAD boost_thread)
    set(LIB_BOOST_LOG boost_log boost_log_setup)
    SET(BOOST_UNIT_TEST_FRAMEWORK boost_unit_test_framework)
endif ()

set (BOOST_LIBRARIES
    ${LIB_BOOST_SYSTEM}
    ${LIB_BOOST_PROGRAM_OPTIONS}
    ${LIB_BOOST_SERIALIZATION}
    ${LIB_BOOST_FILESYSTEM}
    ${LIB_BOOST_DATE_TIME}
    ${LIB_BOOST_IOSTREAMS}
    ${LIB_BOOST_REGEX}
    ${LIB_BOOST_CONTEXT}
    ${LIB_BOOST_COROUTINE}
    ${LIB_BOOST_CHRONO}
    ${LIB_BOOST_LOG}
    ${LIB_BOOST_THREAD}
)

if (UNIX)
    set(THREADLIBS pthread)
endif()

if (UNIX)
  # For now, assume we use g++/clang++
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG=1 -o3 ")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG=1 -D_DEBUG=1 -o0 -g ")
    add_definitions(-D__USE_LARGEFILE64=1 -D__USE_FILE_OFFSET64=1 -D BOOST_LOG_DYN_LINK -Wall -fPIC -std=c++1y -pthread)
endif()


add_executable(shinysocks Manager.cpp Listener.cpp Proxy.cpp main.cpp)
target_compile_definitions(shinysocks PUBLIC -DBOOST_COROUTINE_NO_DEPRECATION_WARNING=1)
target_compile_definitions(shinysocks PUBLIC -DBOOST_COROUTINES_NO_DEPRECATION_WARNING=1)

if (UNIX)
    target_link_libraries(shinysocks ${BOOST_LIBRARIES} ${THREADLIBS})
elseif (WIN32)
    target_link_libraries(shinysocks PRIVATE Boost::boost ${BOOST_LIBRARIES})
endif()


install(TARGETS shinysocks RUNTIME DESTINATION bin)

