add_executable(encrypted-core-tests
    test_core_modules.cpp
    ${CMAKE_SOURCE_DIR}/ClientSecKey/ClientSecKey/AesGcmCrypto.cpp
    ${CMAKE_SOURCE_DIR}/ClientSecKey/ClientSecKey/Base64Util.cpp
    ${CMAKE_SOURCE_DIR}/ClientSecKey/ClientSecKey/Codec.cpp
    ${CMAKE_SOURCE_DIR}/ClientSecKey/ClientSecKey/V2RequestCodec.cpp
    ${CMAKE_SOURCE_DIR}/ClientSecKey/ClientSecKey/V2RespondCodec.cpp
)

target_include_directories(encrypted-core-tests BEFORE PRIVATE
    ${DEMO_PROTO_DIR}
    ${CMAKE_SOURCE_DIR}/ClientSecKey/ClientSecKey
)
add_dependencies(encrypted-core-tests encrypted-protocol)
target_link_libraries(encrypted-core-tests PRIVATE
    OpenSSL::Crypto
    encrypted-protocol
    Threads::Threads
)

add_test(NAME encrypted-core-tests COMMAND encrypted-core-tests)

add_executable(encrypted-message-permission-tests
    test_message_permissions.cpp
    ${CMAKE_SOURCE_DIR}/ServerSeckey/ServerSeckey/MessageQueryService.cpp
    ${CMAKE_SOURCE_DIR}/ServerSeckey/ServerSeckey/Logger.cpp
)

target_include_directories(encrypted-message-permission-tests BEFORE PRIVATE
    ${DEMO_PROTO_DIR}
    ${CMAKE_SOURCE_DIR}/ServerSeckey/ServerSeckey
)
add_dependencies(encrypted-message-permission-tests encrypted-protocol)
target_link_libraries(encrypted-message-permission-tests PRIVATE
    encrypted-protocol
    Threads::Threads
)

add_test(NAME encrypted-message-permission-tests COMMAND encrypted-message-permission-tests)
