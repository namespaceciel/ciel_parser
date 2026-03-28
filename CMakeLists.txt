cmake_minimum_required(VERSION 3.11)
project(ciel_parser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose build type: Debug/Release/RelWithDebInfo/MinSizeRel." FORCE)
endif (NOT CMAKE_BUILD_TYPE)

add_compile_options(-Wno-error)

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
        GIT_TAG v11.1.0
)
FetchContent_MakeAvailable(quill)

FetchContent_Declare(
        tgbotxx
        GIT_REPOSITORY https://github.com/baderouaich/tgbotxx.git
        GIT_TAG v1.2.9.5
)
FetchContent_MakeAvailable(tgbotxx)

FetchContent_Declare(
        minimp4
        GIT_REPOSITORY https://github.com/lieff/minimp4.git
        GIT_TAG master
)
FetchContent_MakeAvailable(minimp4)

FetchContent_Declare(
        Boost
        URL https://github.com/boostorg/boost/releases/download/boost-1.90.0/boost-1.90.0-cmake.tar.xz
        URL_HASH SHA256=aca59f889f0f32028ad88ba6764582b63c916ce5f77b31289ad19421a96c555f
)
FetchContent_MakeAvailable(Boost)

add_library(ciel_parser INTERFACE)
target_include_directories(ciel_parser INTERFACE ${CMAKE_SOURCE_DIR}/include ${minimp4_SOURCE_DIR})
target_link_libraries(ciel_parser INTERFACE
        nlohmann_json
        quill
        tgbotxx
)

add_executable(ciel_parser_bot app/bot.cpp)
target_link_libraries(ciel_parser_bot PRIVATE ciel_parser Boost::process)

add_executable(ciel_parser_test test/test.cpp)
target_link_libraries(ciel_parser_test PRIVATE ciel_parser)
