cmake_minimum_required(VERSION 3.20)
project(souls_npc_example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SMC_EXECUTABLE "" CACHE FILEPATH "Path to the built smc_cpp executable")
if(NOT EXISTS "${SMC_EXECUTABLE}" OR IS_DIRECTORY "${SMC_EXECUTABLE}")
    message(FATAL_ERROR "Set SMC_EXECUTABLE to the built smc_cpp executable")
endif()

set(generated_header "${CMAKE_CURRENT_BINARY_DIR}/SoulsNpc.hpp")
set(generated_source "${CMAKE_CURRENT_BINARY_DIR}/SoulsNpc.cpp")
add_custom_command(
    OUTPUT "${generated_header}" "${generated_source}"
    COMMAND "${SMC_EXECUTABLE}" -c++ -d "${CMAKE_CURRENT_BINARY_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/SoulsNpc.sm"
    DEPENDS "${SMC_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/SoulsNpc.sm"
    VERBATIM)

add_library(souls_npc_machine npc.cpp "${generated_source}" "${generated_header}")
target_include_directories(souls_npc_machine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include/headers/c++")

add_executable(souls_npc main.cpp)
target_link_libraries(souls_npc PRIVATE souls_npc_machine)

include(CTest)
if(BUILD_TESTING)
    add_executable(souls_npc_tests test_npc.cpp)
    target_link_libraries(souls_npc_tests PRIVATE souls_npc_machine)
    add_test(NAME souls_npc_state_machine COMMAND souls_npc_tests)
    add_test(NAME souls_npc_cli
        COMMAND "${CMAKE_COMMAND}"
            "-DEXAMPLE=$<TARGET_FILE:souls_npc>"
            "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/test_cli_input.txt"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cli.cmake")
endif()
