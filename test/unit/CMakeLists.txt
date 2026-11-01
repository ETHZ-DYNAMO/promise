include(FetchContent)
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
  URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926
)
# Don't install GoogleTest with Promise.
set(INSTALL_GTEST OFF)
FetchContent_MakeAvailable(googletest)

add_promise_binary(promise-unit-tests OptimizationTest.cpp)
target_link_libraries(promise-unit-tests PRIVATE GTest::gtest)

include(GoogleTest)
gtest_discover_tests(promise-unit-tests
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
