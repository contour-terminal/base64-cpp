cmake_minimum_required(VERSION 3.14)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(base64-cpp VERSION "0.1.0" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BASE64_CPP_TESTING "base64-cpp: Enable unit tests." ON)

include(ThirdParties)

# ------------------------------------------------------------------------------
set(base64_cpp_SOURCES
    include/base64-cpp/detail/decode-common.hpp
    include/base64-cpp/detail/decode-sse.hpp
    include/base64-cpp/decode.hpp
)
add_library(base64-cpp INTERFACE)
target_compile_features(base64-cpp INTERFACE cxx_std_17)
target_include_directories(base64-cpp INTERFACE
    $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/include>
)

# ------------------------------------------------------------------------------
if(BASE64_CPP_TESTING)
    enable_testing()
    add_executable(test-base64-decoding test/test-main.cpp test/test-base64-decoding.cpp)
    target_link_libraries(test-base64-decoding base64-cpp fmt::fmt-header-only range-v3 Catch2::Catch2)
    add_test(test-base64-decoding test-base64-decoding)
endif()
