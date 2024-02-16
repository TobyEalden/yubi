cmake_minimum_required(VERSION 3.22)

if (NOT OPENSSL_ROOT_DIR)
    set(OPENSSL_ROOT_DIR "${TDXVOLT_ROOT}/openssl-release" CACHE PATH "OpenSSL root")
endif ()

message(STATUS "OpenSSL root: ${OPENSSL_ROOT_DIR}")

set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)

# Check if OpenSSL was found
if (NOT OpenSSL_FOUND)
    message(FATAL_ERROR "OpenSSL not found")
endif ()

message(STATUS "OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
message(STATUS "OpenSSL SSL library: ${OPENSSL_SSL_LIBRARY}")
message(STATUS "OpenSSL Crypto library: ${OPENSSL_CRYPTO_LIBRARY}")

project(yubi)

add_subdirectory(yubi-client)