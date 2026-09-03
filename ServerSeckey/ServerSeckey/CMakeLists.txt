find_path(
    MYSQL_INCLUDE_DIR
    NAMES mysql/mysql.h
    DOC "MySQL 客户端头文件根目录"
)

find_library(
    MYSQL_LIBRARY
    NAMES mysqlclient mariadb
    DOC "MySQL 客户端库文件"
)

if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARY)
    message(FATAL_ERROR "未找到 MySQL 客户端开发库。请设置 MYSQL_INCLUDE_DIR 和 MYSQL_LIBRARY。")
endif()

add_executable(encrypted-server
    main.cpp
    ServerOP.cpp
    MessageService.cpp
    MessageQueryService.cpp
    MessageRepository.cpp
    AuditService.cpp
    mysqlOP.cpp
    Config.cpp
    Codec.cpp
    V2RequestCodec.cpp
    V2RespondCodec.cpp
    EpollServer.cpp
    ThreadPool.cpp
    TcpServer.cpp
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

target_include_directories(encrypted-server BEFORE PRIVATE
    ${DEMO_PROTO_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${MYSQL_INCLUDE_DIR}
)
add_dependencies(encrypted-server encrypted-protocol)
target_link_libraries(encrypted-server PRIVATE
    OpenSSL::Crypto
    encrypted-protocol
    PkgConfig::JSONCPP
    Threads::Threads
    ${MYSQL_LIBRARY}
)
