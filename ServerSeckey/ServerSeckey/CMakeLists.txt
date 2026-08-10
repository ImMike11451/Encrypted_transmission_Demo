set(MYSQL_INCLUDE_DIR "" CACHE PATH "MySQL 客户端头文件目录")
set(MYSQL_LIBRARY "" CACHE FILEPATH "MySQL 客户端库文件")

if(NOT MYSQL_INCLUDE_DIR)
    find_path(MYSQL_INCLUDE_DIR mysql/mysql.h)
endif()

if(NOT MYSQL_LIBRARY)
    find_library(MYSQL_LIBRARY NAMES mysqlclient mariadb)
endif()

if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARY)
    message(FATAL_ERROR "未找到 MySQL 客户端开发库。请设置 MYSQL_INCLUDE_DIR 和 MYSQL_LIBRARY。")
endif()

file(GLOB SERVER_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(encrypted-server ${SERVER_SOURCES})

target_include_directories(encrypted-server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${MYSQL_INCLUDE_DIR}
)
target_link_libraries(encrypted-server PRIVATE
    OpenSSL::Crypto
    protobuf::libprotobuf
    PkgConfig::JSONCPP
    Threads::Threads
    ${MYSQL_LIBRARY}
)
