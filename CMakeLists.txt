cmake_minimum_required(VERSION 3.16)

project(EncryptedTransmissionDemo LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "本项目使用 epoll 与 System V 共享内存，请在 Linux 或 WSL2 中构建；不支持原生 Windows 构建。")
endif()

option(BUILD_CLIENT "构建客户端" ON)
option(BUILD_SERVER "构建服务端" ON)
option(BUILD_TESTING "构建已有的离线模块测试" OFF)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Protobuf MODULE REQUIRED)
if(NOT Protobuf_PROTOC_EXECUTABLE)
    message(FATAL_ERROR "未找到 protoc，请安装 protobuf-compiler，且与 Protobuf 开发库使用同一版本。")
endif()

if(BUILD_CLIENT OR BUILD_SERVER)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(JSONCPP REQUIRED IMPORTED_TARGET jsoncpp)
endif()

# 两端保留同名协议以兼容原有 Visual Studio 工程；CMake 只生成并链接一份。
set(DEMO_PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/ClientSecKey/ClientSecKey/MessageV2.proto")
set(DEMO_SERVER_PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/ServerSeckey/ServerSeckey/MessageV2.proto")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${DEMO_PROTO_FILE}" "${DEMO_SERVER_PROTO_FILE}")
file(SHA256 "${DEMO_PROTO_FILE}" DEMO_CLIENT_PROTO_HASH)
file(SHA256 "${DEMO_SERVER_PROTO_FILE}" DEMO_SERVER_PROTO_HASH)
if(NOT DEMO_CLIENT_PROTO_HASH STREQUAL DEMO_SERVER_PROTO_HASH)
    message(FATAL_ERROR "客户端与服务端 MessageV2.proto 不一致，请先同步协议文件。")
endif()

set(DEMO_PROTO_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${DEMO_PROTO_DIR}")
add_custom_command(
    OUTPUT "${DEMO_PROTO_DIR}/MessageV2.pb.cc" "${DEMO_PROTO_DIR}/MessageV2.pb.h"
    COMMAND "${Protobuf_PROTOC_EXECUTABLE}"
        "--cpp_out=${DEMO_PROTO_DIR}"
        "--proto_path=${CMAKE_CURRENT_SOURCE_DIR}/ClientSecKey/ClientSecKey"
        "${DEMO_PROTO_FILE}"
    DEPENDS "${DEMO_PROTO_FILE}" "${Protobuf_PROTOC_EXECUTABLE}"
    COMMENT "使用当前 protoc 生成统一消息协议"
    VERBATIM
)
add_library(encrypted-protocol STATIC
    "${DEMO_PROTO_DIR}/MessageV2.pb.cc"
    "${DEMO_PROTO_DIR}/MessageV2.pb.h"
)
target_include_directories(encrypted-protocol PUBLIC "${DEMO_PROTO_DIR}")
target_compile_definitions(encrypted-protocol PUBLIC DEMO_CMAKE_PROTO=1)
target_link_libraries(encrypted-protocol PUBLIC protobuf::libprotobuf)

message(STATUS "Protobuf ${Protobuf_VERSION}; protoc: ${Protobuf_PROTOC_EXECUTABLE}")
message(STATUS "程序输出目录: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

if(BUILD_CLIENT)
    add_subdirectory(ClientSecKey/ClientSecKey)
endif()

if(BUILD_SERVER)
    add_subdirectory(ServerSeckey/ServerSeckey)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
