cmake_minimum_required(VERSION 3.21)

project(LinuPaint
    VERSION 1.0.0
    DESCRIPTION "Classic Paint for Linux"
    HOMEPAGE_URL "https://github.com/maclima/linupaint"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LINUPAINT_APP_ID "io.github.maclima.LinuPaint")

option(LINUPAINT_BUILD_APP "Build the Qt application" ON)
option(LINUPAINT_BUILD_TESTS "Build unit tests" ON)
option(LINUPAINT_BUILD_FUZZERS "Build libFuzzer targets (clang only)" OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(LINUPAINT_BUILD_FUZZERS)
    # Instrument our own code for coverage-guided fuzzing; the harnesses add the fuzzer runtime.
    add_compile_options(-fsanitize=fuzzer-no-link,address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

add_subdirectory(src/raster)
add_subdirectory(src/core)

if(LINUPAINT_BUILD_APP)
    add_subdirectory(src/app)
endif()

if(LINUPAINT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(LINUPAINT_BUILD_FUZZERS)
    add_subdirectory(fuzz)
endif()

if(LINUPAINT_BUILD_APP AND UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(FILES packaging/${LINUPAINT_APP_ID}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES packaging/${LINUPAINT_APP_ID}.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    install(FILES resources/icons/linupaint.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME ${LINUPAINT_APP_ID}.svg)
    install(FILES COPYING README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()

# Native packages: cpack -G DEB / cpack -G RPM
set(CPACK_PACKAGE_NAME linupaint)
set(CPACK_PACKAGE_VENDOR "The LinuPaint contributors")
set(CPACK_PACKAGE_CONTACT "Marco Lima <maclima@users.noreply.github.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Draw and edit pictures like the classic Paint")
set(CPACK_RESOURCE_FILE_LICENSE ${PROJECT_SOURCE_DIR}/COPYING)
set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION graphics)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-image-formats-plugins, qt6-translations-l10n")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
set(CPACK_RPM_PACKAGE_AUTOREQ ON)
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    /usr/share/applications /usr/share/metainfo /usr/share/icons /usr/share/icons/hicolor
    /usr/share/icons/hicolor/scalable /usr/share/icons/hicolor/scalable/apps)
include(CPack)
