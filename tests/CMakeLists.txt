include(FetchContent)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(linupaint_tests
    test_image.cpp
    test_draw.cpp
    test_transform.cpp
    test_codecs.cpp
    test_document.cpp
    test_selection.cpp
    test_tools.cpp
)
target_link_libraries(linupaint_tests PRIVATE linupaint_core Catch2::Catch2WithMain)
catch_discover_tests(linupaint_tests)

# Round-trips our encoders through Qt's decoders when the app (and therefore Qt) is built.
if(LINUPAINT_BUILD_APP)
    find_package(Qt6 REQUIRED COMPONENTS Gui)
    add_executable(linupaint_codec_roundtrip test_codec_roundtrip.cpp)
    target_link_libraries(linupaint_codec_roundtrip PRIVATE linupaint_raster Qt6::Gui Catch2::Catch2WithMain)
    catch_discover_tests(linupaint_codec_roundtrip)
endif()
