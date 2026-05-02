cmake_minimum_required(VERSION 3.10)

project(servidor C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(
    Includes
    Config
    modules/Socket
    modules/HttpParser
    modules/HttpServer/src
    modules/Logs
)

set(COMMON_SOURCES
    Config/Config.c
    modules/Socket/Socket.c
    modules/HttpParser/HttpParser.c
    modules/HttpParser/UriParser.c
    modules/HttpParser/HttpResponseParser.c
    modules/HttpServer/src/Response.c
    modules/HttpServer/src/ResponseSender.c
    modules/HttpServer/src/ResponseBody.c
    modules/HttpServer/src/FileManager.c
    modules/HttpServer/src/FileManagerIO.c
    modules/HttpServer/src/FileManagerUtils.c
    Includes/HttpUtils.c
    modules/Logs/Log.c
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

add_executable(server
    modules/HttpServer/src/main.c
    ${COMMON_SOURCES}
    modules/HttpServer/src/WorkerWS.c
)

add_executable(pibl
    main.c
    ${COMMON_SOURCES}
    modules/Worker/ProxyWorker.c
    modules/Worker/ReplicatorWorker.c
    modules/LoadBalancer/loadBalancer.c
    modules/LoadBalancer/HealthCheck.c
    modules/CacheManager/CacheManager.c
    modules/CacheManager/CacheUtils.c
    modules/CacheManager/CacheCleanWorker.c
    modules/CacheManager/CacheStoreWorker.c
    modules/CacheManager/CacheIO.c
    modules/CacheManager/CacheIndex.c
    modules/Replicator/Replicator.c
)

target_include_directories(pibl PRIVATE
    modules/Worker
    modules/CacheManager
    modules/LoadBalancer
    modules/Replicator
)


find_package(Threads REQUIRED)

target_link_libraries(server
    Threads::Threads
    crypto
)

target_link_libraries(pibl
    Threads::Threads
    crypto
)