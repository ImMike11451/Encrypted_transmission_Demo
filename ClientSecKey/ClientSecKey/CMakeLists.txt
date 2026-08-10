file(GLOB CLIENT_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(encrypted-client ${CLIENT_SOURCES})

target_include_directories(encrypted-client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(encrypted-client PRIVATE
    OpenSSL::Crypto
    protobuf::libprotobuf
    PkgConfig::JSONCPP
    Threads::Threads
)
