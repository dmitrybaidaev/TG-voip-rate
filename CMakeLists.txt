project(Tgvoiprate C CXX)

cmake_minimum_required(VERSION 3.10)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(MACOS YES)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(LINUX YES)
endif()


if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(USE_ADDRESS_SANITIZER ON CACHE BOOL "enable Address sanitizer" FORCE)
    set(DEBUG ON CACHE BOOL "Config=Debug" FORCE)
else()
    set(USE_ADDRESS_SANITIZER OFF CACHE BOOL "disable Address sanitizer" FORCE)
    set(DEBUG OFF CACHE BOOL "Config=Release" FORCE)
endif()

#add_compile_options("-DPLATFORM=MACOS64 -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON")

message(STATUS "AS_LIBRARY=${AS_LIBRARY}")

set(CMAKE_CXX_STANDARD 11)

file(GLOB WRAPPER_SRC "*.cc")
file(GLOB WAV_FILE_SRC "wav_file/*.cc")

set(WEBRTC_DEFINES
  WEBRTC_POSIX
)

set(CUSTOM_DEFINES
  __STDC_CONSTANT_MACROS
  __STDC_FORMAT_MACROS
  $<$<BOOL:${MACOS}>:OS_MACOS=1 OS_LINUX=0>
  $<$<BOOL:${LINUX}>:OS_MACOS=0 OS_LINUX=1>
  $<$<BOOL:${DEBUG}>:DEBUG=1>
  WEBRTC_POSIX
)

if(USE_ADDRESS_SANITIZER)
    message(INFO " Use AddressSanitizer!")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif()

if(NOT DEFINED AS_LIBRARY)
  message(STATUS "AS_LIBRARY not defined, set to YES")
  set(AS_LIBRARY YES)
endif()

if(AS_LIBRARY)
  set(BIN_NAME opus_decoder)
  list(REMOVE_ITEM WRAPPER_SRC "pesqmain_adapter.cc")
  add_library(${BIN_NAME} STATIC ${WRAPPER_SRC} ${WAV_FILE_SRC})
else()
  set(BIN_NAME tgvoiprate)
  add_executable(${BIN_NAME} ${WRAPPER_SRC} ${WAV_FILE_SRC})
endif()


target_include_directories(${BIN_NAME}
  PRIVATE
  wav_file
)

target_compile_options(${BIN_NAME} PRIVATE -Wall)

target_compile_definitions(${BIN_NAME}
  PUBLIC
  ${CUSTOM_DEFINES}
  ${WEBRTC_DEFINES}
)

target_link_libraries(${BIN_NAME}
  PUBLIC
  PESQ-MOS
  ffmpeg
  opusfile
  opus
  ogg
)

add_subdirectory(PESQ-MOS)
add_subdirectory(FFmpeg)
add_subdirectory(opus)
add_subdirectory(opusfile)
add_subdirectory(ogg)