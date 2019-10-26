project(Tgvoiprate C CXX)

cmake_minimum_required(VERSION 3.10)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(MACOS YES)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(LINUX YES)
endif()

#add_compile_options("-DPLATFORM=MACOS64 -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON")
#-DPLATFORM=MACOS64

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
  WEBRTC_POSIX
  )

set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

add_executable(Tgvoiprate ${WRAPPER_SRC} ${WAV_FILE_SRC})

target_include_directories(Tgvoiprate
  PRIVATE
  wav_file
)

target_compile_definitions(Tgvoiprate
  PUBLIC
  ${CUSTOM_DEFINES}
  ${WEBRTC_DEFINES}
  )

target_link_libraries(Tgvoiprate
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