cmake_minimum_required(VERSION 3.13)
project(PESQ-MOS C CXX)

set(CMAKE_CXX_STANDARD 14)

file(GLOB C_SRC "*.c")
file(GLOB CXX_SRC "*.cc")

set(CUSTOM_DEFINES
    __STDC_CONSTANT_MACROS
    __STDC_FORMAT_MACROS
    NDEBUG)

add_library(PESQ-MOS STATIC ${C_SRC} ${CXX_SRC})
target_include_directories(PESQ-MOS
  PUBLIC
  $<INSTALL_INTERFACE:include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)


target_compile_definitions(PESQ-MOS
        PUBLIC
        ${CUSTOM_DEFINES})