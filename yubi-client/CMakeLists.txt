cmake_minimum_required(VERSION 3.27)

project(yubi_client)

set(CMAKE_CXX_STANDARD 17)

add_executable(yubi_client
        main.cpp)

target_link_libraries(yubi_client
        OpenSSL::SSL
        OpenSSL::Crypto)

target_include_directories(yubi_client PRIVATE ${OPENSSL_INCLUDE_DIR})