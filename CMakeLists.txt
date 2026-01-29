cmake_minimum_required(VERSION 3.5)
project(ciel_parser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(FETCHCONTENT_BASE_DIR ${CMAKE_SOURCE_DIR}/third_party)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose build type: Debug/Release/RelWithDebInfo/MinSizeRel." FORCE)
endif (NOT CMAKE_BUILD_TYPE)

include(FetchContent)

FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
        quill
        GIT_REPOSITORY https://github.com/odygrd/quill.git
        GIT_TAG v11.0.2
)
FetchContent_MakeAvailable(quill)

FetchContent_Declare(
        tgbotxx
        GIT_REPOSITORY https://github.com/baderouaich/tgbotxx.git
        GIT_TAG v1.2.9.2
)
FetchContent_MakeAvailable(tgbotxx)

add_library(ciel_parser INTERFACE)
target_include_directories(ciel_parser INTERFACE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(ciel_parser INTERFACE
        nlohmann_json
        quill
        tgbotxx
)

add_executable(ciel_parser_bot app/bot.cpp)
target_link_libraries(ciel_parser_bot PRIVATE ciel_parser)

add_executable(ciel_parser_test test/test.cpp )
target_link_libraries(ciel_parser_test PRIVATE ciel_parser)
