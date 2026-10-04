find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets PrintSupport Svg LinguistTools)
if(UNIX AND NOT APPLE)
    find_package(Qt6 6.4 QUIET COMPONENTS DBus)
endif()

qt_standard_project_setup()

# Everything but main() lives in a library so UI tests can drive the real widgets.
qt_add_library(linupaint_ui STATIC
    qtbridge.h qtbridge.cpp
    editor.h editor.cpp
    canvas.h canvas.cpp
    toolbox.h toolbox.cpp
    colorbox.h colorbox.cpp
    imageio.h imageio.cpp
    dialogs.h dialogs.cpp
    platform.h platform.cpp
    views.h views.cpp
    icons.h icons.cpp
    emoji.h emoji.cpp
    mainwindow.h mainwindow.cpp
)
target_include_directories(linupaint_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

qt_add_executable(linupaint WIN32 main.cpp)

set(LINUPAINT_ICONS
    free-select rect-select eraser fill pick-color magnifier pencil brush
    airbrush text line curve rectangle polygon ellipse rounded-rect linupaint)
list(TRANSFORM LINUPAINT_ICONS APPEND ".svg")
list(TRANSFORM LINUPAINT_ICONS PREPEND "${PROJECT_SOURCE_DIR}/resources/icons/")
qt_add_resources(linupaint_ui "icons"
    PREFIX "/icons"
    BASE "${PROJECT_SOURCE_DIR}/resources/icons"
    FILES ${LINUPAINT_ICONS})

qt_add_translations(linupaint_ui
    TS_FILES
        "${PROJECT_SOURCE_DIR}/translations/linupaint_pt_BR.ts"
        "${PROJECT_SOURCE_DIR}/translations/linupaint_es.ts"
    RESOURCE_PREFIX "/i18n")

target_compile_definitions(linupaint_ui PUBLIC
    LINUPAINT_VERSION="${PROJECT_VERSION}"
    LINUPAINT_APP_ID="${LINUPAINT_APP_ID}")
target_link_libraries(linupaint_ui PUBLIC linupaint_core Qt6::Widgets Qt6::PrintSupport Qt6::Svg)

if(TARGET Qt6::DBus)
    target_link_libraries(linupaint_ui PRIVATE Qt6::DBus)
    target_compile_definitions(linupaint_ui PRIVATE LINUPAINT_HAVE_DBUS)
endif()

target_link_libraries(linupaint PRIVATE linupaint_ui)

include(GNUInstallDirs)
install(TARGETS linupaint RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
