add_library(linupaint_core STATIC
    document.cpp
    selection.cpp
    tools.cpp
)

target_include_directories(linupaint_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(linupaint_core PUBLIC linupaint_raster)
