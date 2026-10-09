cmake_minimum_required(VERSION 3.16)

project(FrameScale VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTORCC ON)

# Keep Qt Creator/Ninja builds responsive on machines with limited RAM.
set(FRAMESCALE_COMPILE_JOBS 2 CACHE STRING "Maximum concurrent C++ compilations")
set_property(GLOBAL APPEND PROPERTY JOB_POOLS
    framescale_compile=${FRAMESCALE_COMPILE_JOBS} framescale_link=1)
set(CMAKE_JOB_POOL_COMPILE framescale_compile)
set(CMAKE_JOB_POOL_LINK framescale_link)


find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets)

qt_standard_project_setup()

# Share the compiled brand resources between the application and UI tests.
qt_add_big_resources(FRAMESCALE_BRAND_OBJECTS assets/brand.qrc)
add_custom_target(FrameScaleBrandResources DEPENDS ${FRAMESCALE_BRAND_OBJECTS})

qt_add_executable(FrameScale
    src/main.cpp
    ${FRAMESCALE_BRAND_OBJECTS}
    src/ui/ComparisonDialog.cpp
    src/ui/ComparisonDialog.h
    src/ui/MediaPreviewWidget.cpp
    src/processing/PreviewFrameReader.cpp
    src/processing/PreviewFrameReader.h
    src/ui/MediaPreviewWidget.h
    src/ui/ExportDialog.cpp
    src/ui/ExportDialog.h
    src/ui/EncoderOptionsWidget.cpp
    src/ui/EncoderOptionsWidget.h
    src/ui/EditorControls.h
    src/ui/EnhancementWidget.h
    src/MainWindow.cpp
    src/MainWindow.h
    src/platform/WindowBackdrop.cpp
    src/platform/WindowBackdrop.h
    src/platform/TaskbarProgress.cpp
    src/platform/TaskbarProgress.h
    src/core/ProcessingOptions.cpp
    src/core/ProcessingOptions.h
    src/core/ModelCatalog.cpp
    src/core/ModelCatalog.h
    src/processing/ProcessingJob.cpp
    src/processing/ProcessingJob.h
    src/processing/RuntimePaths.cpp
    src/processing/RuntimePaths.h
)

add_dependencies(FrameScale FrameScaleBrandResources)
target_include_directories(FrameScale PRIVATE src)
target_link_libraries(FrameScale PRIVATE Qt6::Widgets Qt6::Multimedia Qt6::MultimediaWidgets)

target_compile_definitions(FrameScale PRIVATE
    FRAMESCALE_VERSION="${PROJECT_VERSION}"
)

set(FRAMESCALE_LICENSE_FILES
    runtime/LICENSE-yt-dlp.txt
    runtime/LICENSE-yt-dlp-third-party.txt
    runtime/LICENSE-deno.txt
    runtime/LICENSE-Anime4K.txt
    runtime/LICENSE-GPL-2.0.txt
    runtime/LICENSE-GPL-3.0.txt
    runtime/LICENSE-LGPL-2.1.txt
    runtime/LICENSE-libplacebo-LGPL-2.1.txt
    runtime/LICENSE-mpv-Copyright.txt
    runtime/LICENSE-ncnn-BSD-3-Clause.txt
    runtime/LICENSE-Qt-GPL-3.0.txt
    runtime/LICENSE-Qt-GPL-Exception-1.0.txt
    runtime/LICENSE-Qt-LGPL-3.0.txt
    runtime/LICENSE-Real-CUGAN-models.txt
    runtime/LICENSE-Real-CUGAN-ncnn-vulkan.txt
    runtime/LICENSE-Real-ESRGAN-models.txt
    runtime/LICENSE-Real-ESRGAN-ncnn-vulkan.txt
    runtime/LICENSE-RIFE-models.txt
    runtime/LICENSE-RIFE-ncnn-vulkan.txt
)

if(EXISTS "${CMAKE_SOURCE_DIR}/runtime/bin" AND EXISTS "${CMAKE_SOURCE_DIR}/models")
    add_custom_command(TARGET FrameScale POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:FrameScale>/runtime/bin"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/runtime/bin"
                "$<TARGET_FILE_DIR:FrameScale>/runtime/bin"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/runtime/manifest.json"
                "$<TARGET_FILE_DIR:FrameScale>/runtime/manifest.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/runtime/README.md"
                "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
                "$<TARGET_FILE_DIR:FrameScale>/runtime"
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:FrameScale>/runtime/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${FRAMESCALE_LICENSE_FILES}
                "$<TARGET_FILE_DIR:FrameScale>/runtime/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/models"
                "$<TARGET_FILE_DIR:FrameScale>/models"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Copying FrameScale runtime, models, and licenses"
    )
endif()

if(WIN32)
    target_link_libraries(FrameScale PRIVATE dwmapi shell32 ole32 uuid)
    if(MINGW)
        # Compile icons separately: windres cannot consume Qt's escaped string defines.
        add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/framescale-icon.o"
            COMMAND "${CMAKE_RC_COMPILER}" -i framescale.rc
                    -o "${CMAKE_CURRENT_BINARY_DIR}/framescale-icon.o" -O coff
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            DEPENDS assets/framescale.rc assets/framescale.ico
            VERBATIM)
        set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/framescale-icon.o"
            PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
        target_sources(FrameScale PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/framescale-icon.o")
    else()
        target_sources(FrameScale PRIVATE assets/framescale.rc)
    endif()
endif()

set_target_properties(FrameScale PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

include(GNUInstallDirs)
install(TARGETS FrameScale
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY models
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY runtime/bin
    DESTINATION ${CMAKE_INSTALL_BINDIR}/runtime
)
install(FILES runtime/manifest.json runtime/README.md THIRD_PARTY_NOTICES.md
    DESTINATION ${CMAKE_INSTALL_BINDIR}/runtime
)
install(FILES ${FRAMESCALE_LICENSE_FILES}
    DESTINATION ${CMAKE_INSTALL_BINDIR}/runtime/licenses
)

qt_generate_deploy_app_script(
    TARGET FrameScale
    OUTPUT_SCRIPT framescale_deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${framescale_deploy_script})

enable_testing()

add_executable(FrameScaleCoreTests
    tests/core_tests.cpp
    src/core/ProcessingOptions.cpp
    src/core/ProcessingOptions.h
    src/core/ModelCatalog.cpp
    src/core/ModelCatalog.h
)
target_include_directories(FrameScaleCoreTests PRIVATE src)
add_test(NAME FrameScaleCoreTests COMMAND FrameScaleCoreTests)

qt_add_executable(FrameScaleUiSmoke
    tests/ui_smoke.cpp
    ${FRAMESCALE_BRAND_OBJECTS}
    src/ui/ComparisonDialog.cpp
    src/ui/ComparisonDialog.h
    src/ui/MediaPreviewWidget.cpp
    src/processing/PreviewFrameReader.cpp
    src/processing/PreviewFrameReader.h
    src/ui/MediaPreviewWidget.h
    src/ui/ExportDialog.cpp
    src/ui/ExportDialog.h
    src/ui/EncoderOptionsWidget.cpp
    src/ui/EncoderOptionsWidget.h
    src/ui/EditorControls.h
    src/ui/EnhancementWidget.h
    src/MainWindow.cpp
    src/MainWindow.h
    src/platform/WindowBackdrop.cpp
    src/platform/WindowBackdrop.h
    src/platform/TaskbarProgress.cpp
    src/platform/TaskbarProgress.h
    src/core/ProcessingOptions.cpp
    src/core/ProcessingOptions.h
    src/core/ModelCatalog.cpp
    src/core/ModelCatalog.h
    src/processing/ProcessingJob.cpp
    src/processing/ProcessingJob.h
    src/processing/RuntimePaths.cpp
    src/processing/RuntimePaths.h
)
add_dependencies(FrameScaleUiSmoke FrameScaleBrandResources)
target_include_directories(FrameScaleUiSmoke PRIVATE src)
target_link_libraries(FrameScaleUiSmoke PRIVATE Qt6::Widgets Qt6::Multimedia Qt6::MultimediaWidgets)
target_compile_definitions(FrameScaleUiSmoke PRIVATE
    FRAMESCALE_VERSION="${PROJECT_VERSION}"
)
if(WIN32)
    target_link_libraries(FrameScaleUiSmoke PRIVATE dwmapi shell32 ole32 uuid)
endif()
add_test(NAME FrameScaleUiSmoke COMMAND FrameScaleUiSmoke)
add_test(NAME FrameScaleOrientationUi COMMAND FrameScaleUiSmoke --orientation-only)
set_tests_properties(FrameScaleOrientationUi PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 30
)
set_tests_properties(FrameScaleUiSmoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 180
)

add_executable(FrameScaleFakeTool
    tests/fake_processing_tool.cpp
)
target_link_libraries(FrameScaleFakeTool PRIVATE Qt6::Core Qt6::Gui)

add_executable(FrameScaleProcessingSmoke
    tests/processing_smoke.cpp
    src/core/ProcessingOptions.cpp
    src/core/ProcessingOptions.h
    src/core/ModelCatalog.cpp
    src/core/ModelCatalog.h
    src/processing/ProcessingJob.cpp
    src/processing/ProcessingJob.h
    src/processing/RuntimePaths.cpp
    src/processing/RuntimePaths.h
)
target_include_directories(FrameScaleProcessingSmoke PRIVATE src)
target_link_libraries(FrameScaleProcessingSmoke PRIVATE Qt6::Core Qt6::Gui)
add_dependencies(FrameScaleProcessingSmoke FrameScaleFakeTool)
add_test(
    NAME FrameScaleProcessingSmoke
    COMMAND FrameScaleProcessingSmoke
            --fake-suite
            "$<TARGET_FILE:FrameScaleFakeTool>"
)
set_tests_properties(FrameScaleProcessingSmoke PROPERTIES
    TIMEOUT 120
    LABELS integration
)

# Real GPU engines are opt-in; the default CTest suite remains deterministic.
if(WIN32)
    find_program(FRAMESCALE_POWERSHELL_EXECUTABLE NAMES pwsh powershell)
    if(FRAMESCALE_POWERSHELL_EXECUTABLE)
        add_custom_target(validate-media-pipelines
            COMMAND "${FRAMESCALE_POWERSHELL_EXECUTABLE}"
                    -NoProfile -ExecutionPolicy Bypass
                    -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/validate-media-pipelines.ps1"
                    -BuildDir "$<TARGET_FILE_DIR:FrameScaleProcessingSmoke>"
                    -QtBin "$<TARGET_FILE_DIR:Qt6::Core>"
            DEPENDS FrameScale FrameScaleProcessingSmoke
            VERBATIM
            USES_TERMINAL
            COMMENT "Validating real FrameScale media pipelines (requires Vulkan GPU)"
        )
    endif()
endif()

qt_add_executable(FrameScaleExportQueueSmoke
    tests/export_queue_smoke.cpp
    src/platform/WindowBackdrop.cpp
    src/ui/ExportDialog.cpp
    src/ui/ExportDialog.h
)
target_include_directories(FrameScaleExportQueueSmoke PRIVATE src)
target_link_libraries(FrameScaleExportQueueSmoke PRIVATE Qt6::Widgets)
if(WIN32)
    target_link_libraries(FrameScaleExportQueueSmoke PRIVATE dwmapi)
endif()
add_test(NAME FrameScaleExportQueueSmoke COMMAND FrameScaleExportQueueSmoke)
set_tests_properties(FrameScaleExportQueueSmoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 30
)

# Decoder regression harness: QCoreApplication only; no UI automation.
add_executable(FrameScalePreviewDecodeCheck EXCLUDE_FROM_ALL
    tests/preview_decode_check.cpp
    src/processing/PreviewFrameReader.cpp
    src/processing/PreviewFrameReader.h)
target_include_directories(FrameScalePreviewDecodeCheck PRIVATE src)
target_link_libraries(FrameScalePreviewDecodeCheck PRIVATE Qt6::Core Qt6::Gui)

# Tests remain available as explicit targets, without duplicating the application
# compilation during a normal Qt Creator Build All.
set_target_properties(FrameScaleCoreTests FrameScaleUiSmoke FrameScaleFakeTool
    FrameScaleProcessingSmoke FrameScaleExportQueueSmoke
    PROPERTIES EXCLUDE_FROM_ALL TRUE)
add_custom_target(framescale-tests
    DEPENDS FrameScaleCoreTests FrameScaleUiSmoke FrameScaleProcessingSmoke
            FrameScaleExportQueueSmoke)

# Nonvisual layer command regression harness. No windows or screenshots.
add_executable(FrameScaleLayerStateTests EXCLUDE_FROM_ALL
    tests/layer_state_tests.cpp
    src/platform/WindowBackdrop.cpp
    src/processing/RuntimePaths.cpp)
target_include_directories(FrameScaleLayerStateTests PRIVATE src)
target_link_libraries(FrameScaleLayerStateTests PRIVATE Qt6::Widgets)
if(WIN32)
    target_link_libraries(FrameScaleLayerStateTests PRIVATE dwmapi)
endif()
add_test(NAME FrameScaleLayerStateTests COMMAND FrameScaleLayerStateTests)
set_tests_properties(FrameScaleLayerStateTests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 30)
add_dependencies(framescale-tests FrameScaleLayerStateTests)
