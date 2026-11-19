
cmake_minimum_required(VERSION 3.20)
project(smc_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(smc_core
    src/cpp_generator.cpp
    src/cpp_generator_internal.cpp
    src/generator.cpp
    src/lexer.cpp
    src/parser.cpp
    src/syntax_checker.cpp
)
target_include_directories(smc_core PUBLIC include)
target_compile_features(smc_core PUBLIC cxx_std_20)

add_executable(smc_cpp src/main.cpp)
target_link_libraries(smc_cpp PRIVATE smc_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(smc_tests tests/test_smc.cpp)
    target_link_libraries(smc_tests PRIVATE smc_core)
    add_test(NAME smc_unit_tests COMMAND smc_tests)
    add_executable(smc_cpp_fixture_generator tests/generate_cpp_fixture.cpp)
    target_link_libraries(smc_cpp_fixture_generator PRIVATE smc_core)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cpp_fixture_generated.hpp
               ${CMAKE_CURRENT_BINARY_DIR}/cpp_fixture_generated.cpp
        COMMAND smc_cpp_fixture_generator ${CMAKE_CURRENT_SOURCE_DIR}/tests/cpp_fixture.sm
                ${CMAKE_CURRENT_BINARY_DIR}/cpp_fixture_generated.hpp
                ${CMAKE_CURRENT_BINARY_DIR}/cpp_fixture_generated.cpp
        DEPENDS smc_cpp_fixture_generator tests/cpp_fixture.sm
        VERBATIM)
    add_executable(smc_generated_cpp_test tests/test_generated_cpp.cpp
                   ${CMAKE_CURRENT_BINARY_DIR}/cpp_fixture_generated.cpp
                   ${CMAKE_CURRENT_BINARY_DIR}/cpp_fixture_generated.hpp)
    target_include_directories(smc_generated_cpp_test PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/include/headers/c++)
    add_test(NAME smc_generated_cpp_test COMMAND smc_generated_cpp_test)
    add_test(NAME smc_cli_valid
             COMMAND smc_cpp -c++ --check ${CMAKE_CURRENT_SOURCE_DIR}/tests/valid.sm)
    add_test(NAME smc_cli_invalid
             COMMAND smc_cpp -c++ --check ${CMAKE_CURRENT_SOURCE_DIR}/tests/invalid.sm)
    add_test(NAME smc_cli_lexer_grammar
             COMMAND smc_cpp -c++ --check ${CMAKE_CURRENT_SOURCE_DIR}/examples/SMC/SmcLexer.sm)
    add_test(NAME smc_cli_parser_grammar
             COMMAND smc_cpp -c++ --check ${CMAKE_CURRENT_SOURCE_DIR}/examples/SMC/SmcParser.sm)
    add_test(NAME smc_cli_generate
             COMMAND ${CMAKE_COMMAND}
                     "-DSMC_EXECUTABLE=$<TARGET_FILE:smc_cpp>"
                     "-DSMC_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
                     "-DTEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_generation"
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli_generation.cmake)
    set_tests_properties(smc_cli_invalid PROPERTIES WILL_FAIL TRUE)
endif()
