cmake_minimum_required(VERSION 3.21)
project(punchpixel-host-tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if("$ENV{UNA_SDK}" STREQUAL "")
    message(FATAL_ERROR "UNA_SDK environment variable is not set.")
endif()

# Same pinned GoogleTest the SDK's own host tests use.
include($ENV{UNA_SDK}/Tests/Host/cmake/FetchGoogleTest.cmake)

set(LIBS "${CMAKE_CURRENT_LIST_DIR}/../../Software/Libs")

add_executable(punchpixel-host-tests
    Game_test.cpp
    Scene_test.cpp
    Perf_test.cpp
    Audio_test.cpp
    "${LIBS}/Sources/Game.cpp"
    "${LIBS}/Sources/Painter.cpp"
    "${LIBS}/Sources/Scene.cpp"
    "${LIBS}/Sources/Audio.cpp"
)
target_include_directories(punchpixel-host-tests PRIVATE "${LIBS}/Header")
target_link_libraries(punchpixel-host-tests PRIVATE GTest::gtest_main)

enable_testing()
add_test(NAME punchpixel-host-tests COMMAND punchpixel-host-tests)
