cmake_minimum_required(VERSION 3.20)
project(tenuki_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
if(ENABLE_ASAN AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

add_library(tenuki
    src/go/Board.cpp
    src/go/Rules.cpp
    src/go/Zobrist.cpp
    src/gtp/GTP.cpp
    src/search/Search.cpp
    src/sgf/SGF.cpp
)

target_include_directories(tenuki PUBLIC include)

# Optional warnings preset
option(TENUKI_ENABLE_WARNINGS "Enable common compiler warnings" ON)
option(TENUKI_WERROR "Treat warnings as errors" OFF)
if(TENUKI_ENABLE_WARNINGS)
  set(_TENUKI_WARNINGS -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
    list(APPEND _TENUKI_WARNINGS -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-unused-parameter)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND _TENUKI_WARNINGS -Wno-unused-parameter)
  endif()
  if(TENUKI_WERROR)
    list(APPEND _TENUKI_WARNINGS -Werror)
  endif()
  target_compile_options(tenuki PRIVATE ${_TENUKI_WARNINGS})
endif()

add_executable(tenuki_cli src/main.cpp)
target_link_libraries(tenuki_cli PRIVATE tenuki)
if(TENUKI_ENABLE_WARNINGS)
  target_compile_options(tenuki_cli PRIVATE ${_TENUKI_WARNINGS})
endif()

add_executable(tenuki_tests
    tests/BoardTests.cpp
    tests/SearchTests.cpp
    tests/SGFTests.cpp
    tests/SGFFuzzTests.cpp
    tests/SearchStressTests.cpp
    tests/TestMain.cpp
)

target_link_libraries(tenuki_tests PRIVATE tenuki)
if(TENUKI_ENABLE_WARNINGS)
  target_compile_options(tenuki_tests PRIVATE ${_TENUKI_WARNINGS})
endif()

enable_testing()
add_test(NAME all_tests COMMAND tenuki_tests)

# Integration test: GTP over stdio using Python
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME gtp_integration
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/GTPIntegration_test.py $<TARGET_FILE:tenuki_cli>
  )
  add_test(
    NAME gtp_fuzz
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/GTPFuzz_test.py $<TARGET_FILE:tenuki_cli>
  )
endif()
include(CMakeDependentOption)
include(CTest)

option(ENABLE_COVERAGE "Enable coverage instrumentation" OFF)
if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  message(STATUS "Coverage instrumentation enabled")
  add_compile_options(-O0 -g --coverage)
  add_link_options(--coverage)
endif()
