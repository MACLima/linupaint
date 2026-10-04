# libFuzzer targets (clang): cmake -DLINUPAINT_BUILD_FUZZERS=ON -DCMAKE_CXX_COMPILER=clang++
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "LINUPAINT_BUILD_FUZZERS requires clang")
endif()

set(FUZZ_FLAGS -fsanitize=fuzzer,address,undefined)

# Opening pictures: every format reader the app uses, fed through the same entry point.
add_executable(fuzz_load_image fuzz_load_image.cpp)
target_link_libraries(fuzz_load_image PRIVATE linupaint_ui)
target_compile_options(fuzz_load_image PRIVATE ${FUZZ_FLAGS})
target_link_options(fuzz_load_image PRIVATE ${FUZZ_FLAGS})

# Our own encoders and drawing primitives with arbitrary sizes and coordinates.
add_executable(fuzz_raster fuzz_raster.cpp)
target_link_libraries(fuzz_raster PRIVATE linupaint_core)
target_compile_options(fuzz_raster PRIVATE ${FUZZ_FLAGS})
target_link_options(fuzz_raster PRIVATE ${FUZZ_FLAGS})
