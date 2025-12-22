cmake_minimum_required(VERSION 3.14)
project(nstd)
enable_language(CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Library
add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Install rules
install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    INCLUDES DESTINATION include
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION include)

install(EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION lib/cmake/${PROJECT_NAME}
)

# Testing
option(NSTD_ENABLE_TESTING "Enable tests for the nstd library" ON)
if(NSTD_ENABLE_TESTING)
    find_package(googletest QUIET)
    if (NOT googletest_FOUND)
        include(FetchContent)
        FetchContent_Declare(
                googletest
                URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        )
        # For Windows: Prevent overriding the parent project's compiler/linker settings
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    enable_testing()

    file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp")

    add_executable(tests ${TEST_SOURCES})

    target_link_libraries(tests PRIVATE nstd GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(tests)
endif()
