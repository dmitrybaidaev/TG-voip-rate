project(opusfile C CXX)

file(GLOB C_SRC "src/*.c")

add_library(opusfile STATIC ${C_SRC})

target_include_directories(opusfile
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(opusfile
  PRIVATE
  ogg
  opus
  )
