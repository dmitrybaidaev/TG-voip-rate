project(swresample C)

set(CUSTOM_DEFINES
    # __STDC_CONSTANT_MACROS
    # __STDC_FORMAT_MACROS
    NDEBUG)

add_library(swresample STATIC 
    audioconvert.c
    dither.c
    options.c
    rematrix.c
    resample.c
    resample_dsp.c
    swresample.c
    swresample_frame.c
    soxr_resample.c
)

target_include_directories(swresample
  PUBLIC
  .
  ..
  ../libavutil
  ../config/macos
  # $<$<BOOL:${APPLE}>:../config/macos>
)

target_compile_definitions(swresample
        PUBLIC
        ${CUSTOM_DEFINES})