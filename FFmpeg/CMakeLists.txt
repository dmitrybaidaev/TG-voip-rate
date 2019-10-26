project(ffmpeg C CXX)

include(libavutil-src.txt)
include(libswresample-src.txt)

set(ffmpeg_defines
  HAVE_AV_CONFIG_H
  FFMPEG_CONFIGURATION=NULL)

add_library(ffmpeg STATIC 
  ${libavutil_src}
  ${libswresample_src}
  )

target_include_directories(ffmpeg
  PUBLIC
  $<$<BOOL:${MACOS}>:${CMAKE_CURRENT_SOURCE_DIR}/config/macos>
  $<$<BOOL:${LINUX}>:${CMAKE_CURRENT_SOURCE_DIR}/config/linux>
  ${CMAKE_CURRENT_SOURCE_DIR}/.
)

target_compile_definitions(ffmpeg
  PUBLIC
  ${ffmpeg_defines}
 )