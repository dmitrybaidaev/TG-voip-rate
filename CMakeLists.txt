cmake_minimum_required(VERSION 3.13)
project(Tgvoiprate C CXX)

set(CMAKE_CXX_STANDARD 14)

file(GLOB CXX_SRC "*.cc")

set(CUSTOM_DEFINES
    __STDC_CONSTANT_MACROS
    __STDC_FORMAT_MACROS
    NDEBUG)

# find_library(LIB_AVRESAMPLE libavresample.a)
# find_library(LIB_SWRESAMPLE libswresample.a)
# find_library(LIB_AVCODEC    libavcodec.a)
# find_library(LIB_AVDEVICE   libavdevice.a)
# find_library(LIB_AVFILTER   libavfilter.a)
# find_library(LIB_AVFORMAT   libavformat.a)
# find_library(LIB_AVUTIL     libavutil.a)
# find_library(LIB_POSTPROC   libpostproc.a)
# find_library(LIB_SWSCALE    libswscale.a)

include_directories(/usr/local/include)

add_executable(Tgvoiprate ${C_SRC} ${CXX_SRC})

target_compile_definitions(Tgvoiprate
        PUBLIC
        ${CUSTOM_DEFINES})

target_link_libraries(Tgvoiprate
  PUBLIC
  PESQ-MOS
  swresample
  )


#target_link_libraries(PESQ-MOS
#        PRIVATE
#        ${LIB_AVRESAMPLE}
#        ${LIB_SWRESAMPLE}
#        ${LIB_AVCODEC}
#        ${LIB_AVDEVICE}
#        ${LIB_AVFILTER}
#        ${LIB_AVFORMAT}
#        ${LIB_AVUTIL}
#        ${LIB_POSTPROC}
#        ${LIB_SWSCALE}
#        )


add_subdirectory(PESQ-MOS)
add_subdirectory(FFmpeg)