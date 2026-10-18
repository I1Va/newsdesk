add_executable(newsdesk_tests
    test_main.cpp
)
target_link_libraries(newsdesk_tests PRIVATE newsdesk_core doctest::doctest)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(newsdesk_tests)
