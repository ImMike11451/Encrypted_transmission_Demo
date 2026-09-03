add_executable(encrypted-client
    main.cpp
    ClientOP.cpp
    MessageClient.cpp
    Config.cpp
    Codec.cpp
    V2RequestCodec.cpp
    V2RespondCodec.cpp
    TcpSocket.cpp
    SecKeyShm.cpp
    BaseShm.cpp
    AesGcmCrypto.cpp
    AesCrypto.cpp
    RsaCrypto.cpp
    Hash.cpp
    Base64Util.cpp
    Logger.cpp
)

target_include_directories(encrypted-client BEFORE PRIVATE
    "${DEMO_PROTO_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
add_dependencies(encrypted-client encrypted-protocol)
target_link_libraries(encrypted-client PRIVATE
    OpenSSL::Crypto
    encrypted-protocol
    PkgConfig::JSONCPP
    Threads::Threads
)
