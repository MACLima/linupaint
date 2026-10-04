add_library(linupaint_raster STATIC
    image.cpp
    draw.cpp
    fill.cpp
    transform.cpp
    quantize.cpp
    codecs.cpp
)

target_include_directories(linupaint_raster PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
