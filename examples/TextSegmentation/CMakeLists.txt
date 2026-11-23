cmake_minimum_required(VERSION 3.20)
project(text_segmentation_example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(uni_algo
    GIT_REPOSITORY https://github.com/uni-algo/uni-algo.git
    GIT_TAG 56710d1297be7db7bbac6d577aedc87ee686334b)
FetchContent_MakeAvailable(uni_algo)

set(SMC_EXECUTABLE "" CACHE FILEPATH "Path to the built smc_cpp executable")
if(NOT EXISTS "${SMC_EXECUTABLE}" OR IS_DIRECTORY "${SMC_EXECUTABLE}")
    message(FATAL_ERROR "Set SMC_EXECUTABLE to the built smc_cpp executable")
endif()

set(generated_header "${CMAKE_CURRENT_BINARY_DIR}/TextSegmentation_v2.hpp")
set(generated_source "${CMAKE_CURRENT_BINARY_DIR}/TextSegmentation_v2.cpp")
add_custom_command(
    OUTPUT "${generated_header}" "${generated_source}"
    COMMAND "${SMC_EXECUTABLE}" -c++ -d "${CMAKE_CURRENT_BINARY_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/TextSegmentation_v2.sm"
    DEPENDS "${SMC_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/TextSegmentation_v2.sm"
    VERBATIM)

add_library(segmentation segmentation.cpp "${generated_header}" "${generated_source}")
target_link_libraries(segmentation PRIVATE uni-algo::uni-algo)
target_include_directories(segmentation PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include/headers/c++")

add_library(sentencepiece_lite sentence_piece_lite/sentencepiece_lite.cc)
target_include_directories(sentencepiece_lite PUBLIC sentence_piece_lite)

add_executable(text_segmentation main.cpp)
target_link_libraries(text_segmentation PRIVATE segmentation sentencepiece_lite)

include(CTest)
if(BUILD_TESTING)
    add_executable(text_segmentation_tests test_segmentation.cpp)
    target_link_libraries(text_segmentation_tests PRIVATE segmentation sentencepiece_lite)
    add_test(NAME text_segmentation_tests
             COMMAND text_segmentation_tests "${CMAKE_CURRENT_SOURCE_DIR}/sentence_piece_lite/mt5.spm.fb")
endif()
