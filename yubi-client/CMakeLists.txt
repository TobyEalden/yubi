cmake_minimum_required(VERSION 3.27)

project(yubi_client)

set(CMAKE_CXX_STANDARD 17)

if (NOT OPENSSL_ROOT_DIR)
    set(OPENSSL_ROOT_DIR "${TDXVOLT_ROOT}/openssl-release" CACHE PATH "OpenSSL root")
endif ()

set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)

# Check if OpenSSL was found
if (NOT OpenSSL_FOUND)
    message(FATAL_ERROR "OpenSSL not found")
endif ()

message(STATUS "OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
message(STATUS "OpenSSL SSL library: ${OPENSSL_SSL_LIBRARY}")
message(STATUS "OpenSSL Crypto library: ${OPENSSL_CRYPTO_LIBRARY}")

add_executable(yubi_client
        main.cpp)

target_link_libraries(yubi_client
        OpenSSL::SSL
        OpenSSL::Crypto)

target_include_directories(yubi_client PRIVATE ${OPENSSL_INCLUDE_DIR})