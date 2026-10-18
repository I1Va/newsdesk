# newsdesk_core: платформо-независимая логика (core + protocol + формат storage).
add_library(newsdesk_core STATIC
    core/clock.cpp
    core/story.cpp
    core/trust.cpp
    core/registry.cpp
    core/stats.cpp
    core/engine.cpp
    core/outbox.cpp
    protocol/message.cpp
    storage/journal.cpp
)
target_include_directories(newsdesk_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(newsdesk_core PUBLIC nlohmann_json::nlohmann_json)

# newsdesk_platform: всё, что зависит от ОС (сокеты, poll, fsync, сигналы).
add_library(newsdesk_platform STATIC
    platform/platform_posix.cpp
)
target_include_directories(newsdesk_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(newsdesk
    main.cpp
    server/server.cpp
    client/client.cpp
)
target_link_libraries(newsdesk PRIVATE newsdesk_core newsdesk_platform)
