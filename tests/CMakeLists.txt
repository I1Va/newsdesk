add_executable(newsdesk_tests
    test_main.cpp
    test_trust.cpp
    test_story.cpp
    test_registry.cpp
    test_outbox.cpp
    test_stats.cpp
    test_engine.cpp
    test_protocol.cpp
)
target_link_libraries(newsdesk_tests PRIVATE newsdesk_core doctest::doctest)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(newsdesk_tests)
