cmake_minimum_required(VERSION 3.16)

project(EncryptedTransmissionDemo LANGUAGES CXX)

option(BUILD_CLIENT "构建客户端" ON)
option(BUILD_SERVER "构建服务端" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Protobuf REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED IMPORTED_TARGET jsoncpp)

if(BUILD_CLIENT)
    add_subdirectory(ClientSecKey/ClientSecKey)
endif()

if(BUILD_SERVER)
    add_subdirectory(ServerSeckey/ServerSeckey)
endif()
