cmake_minimum_required(VERSION 3.21)

project(Miru VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 QuickTemplates2 Svg Network WebSockets)
# QZipReader/QZipWriter live in QtCore's private API (stable for many years).
# Qt 6.9+ ships private modules as separate packages; on 6.8 the
# Qt6::CorePrivate target comes with Core.
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.9)
    find_package(Qt6 REQUIRED COMPONENTS CorePrivate)
endif()

qt_standard_project_setup(REQUIRES 6.8)

add_subdirectory(src/core)
add_subdirectory(src/collab)

# Miru's own Qt Quick Controls style (rounded fields, buttons, menus, dialogs).
# Colours come from the window palette, which follows the light / dark theme.
qt_add_library(mirustyle STATIC)
qt_add_qml_module(mirustyle
    URI MiruStyle
    VERSION 1.0
    IMPORTS QtQuick.Controls.Basic
    QML_FILES
        style/MiruStyle/Button.qml
        style/MiruStyle/ToolButton.qml
        style/MiruStyle/TextField.qml
        style/MiruStyle/TextArea.qml
        style/MiruStyle/CheckBox.qml
        style/MiruStyle/ComboBox.qml
        style/MiruStyle/SpinBox.qml
        style/MiruStyle/Slider.qml
        style/MiruStyle/Dialog.qml
        style/MiruStyle/DialogButtonBox.qml
        style/MiruStyle/Popup.qml
        style/MiruStyle/Menu.qml
        style/MiruStyle/MenuItem.qml
        style/MiruStyle/MenuSeparator.qml
        style/MiruStyle/ToolTip.qml
)
target_link_libraries(mirustyle PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::QuickTemplates2)

qt_add_executable(Miru
    src/app/main.cpp
)

set_source_files_properties(qml/Theme.qml qml/ToolRegistry.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(Miru
    URI Miru
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/ToolButtonIcon.qml
        qml/PropertiesPanel.qml
        qml/ExportDialog.qml
        qml/ColorSwatches.qml
        qml/SignInPage.qml
        qml/PasswordDialog.qml
        qml/ThemeButton.qml
        qml/BoardContextMenu.qml
        qml/ImageToolbar.qml
        qml/HomePage.qml
        qml/LiveSessionDialog.qml
        qml/ToolRegistry.qml
        qml/ToolFlyout.qml
        qml/ToolOptions.qml
        qml/ShapeLibraryPanel.qml
        qml/IconsPanel.qml
        qml/LayersPanel.qml
        qml/TemplatesDialog.qml
        qml/ToolsPanel.qml qml/CommentPopup.qml qml/LinkDialog.qml qml/SavedFilesDialog.qml qml/ShortcutsDialog.qml qml/CursorChat.qml qml/ConnectorToolbar.qml qml/MiruLogo.qml
    RESOURCES
        assets/miru-logo.svg
        assets/miru-icon.svg
    SOURCES
        src/app/BoardCanvas.h src/app/BoardCanvas.cpp src/app/BoardCanvasExtras.cpp src/app/BoardCanvasMenus.cpp
        src/app/ExportController.h src/app/ExportController.cpp
        src/app/AppState.h src/app/AppState.cpp
        src/app/CollabController.h src/app/CollabController.cpp
        src/app/CloudStore.h src/app/CloudStore.cpp
        src/app/TemplatePreviewProvider.h
        src/app/LibraryPreviewProvider.h
)

target_link_libraries(Miru PRIVATE
    boardcore
    mirustyleplugin
    collab
    Qt6::WebSockets
    Qt6::Network
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Svg
)

target_include_directories(Miru PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/app)

# Bundled colour-emoji font, used only where the system has none (e.g. the
# browser build). Drop a COLR emoji .ttf at fonts/emoji.ttf to enable it.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/fonts/emoji.ttf)
    qt_add_resources(Miru "emojifont" PREFIX "/fonts" BASE fonts FILES fonts/emoji.ttf)
endif()

target_compile_definitions(Miru PRIVATE PROJECT_VERSION="${PROJECT_VERSION}")

set_target_properties(Miru PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.miru.app"
    MACOSX_BUNDLE_BUNDLE_NAME "Miru"
    MACOSX_BUNDLE_COPYRIGHT "Copyright (C) miru.com"
)

# Publisher shown in Windows file properties, installers and the About box.
set(MIRU_PUBLISHER "miru.com")
string(TIMESTAMP MIRU_YEAR "%Y")
target_compile_definitions(Miru PRIVATE MIRU_PUBLISHER="${MIRU_PUBLISHER}")
if(WIN32)
    set(MIRU_ICO "${CMAKE_CURRENT_SOURCE_DIR}/assets/miru.ico")
    file(TO_CMAKE_PATH "${MIRU_ICO}" MIRU_ICO)
    configure_file(src/app/miru.rc.in ${CMAKE_CURRENT_BINARY_DIR}/miru.rc @ONLY)
    target_sources(Miru PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/miru.rc)
endif()

if(EMSCRIPTEN)
    # Browser open/download dialogs (QWasmLocalFileAccess) live in QtGui's private API.
    if(Qt6_VERSION VERSION_GREATER_EQUAL 6.9)
        find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
    endif()
    target_link_libraries(Miru PRIVATE Qt6::GuiPrivate)
    # Miru's own start page (animated logo instead of Qt's loading screen). Qt still
    # writes its default Miru.html; index.html is the page we publish.
    add_custom_command(TARGET Miru POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/web/index.html $<TARGET_FILE_DIR:Miru>/index.html
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/assets/miru-icon.svg $<TARGET_FILE_DIR:Miru>/miru-icon.svg
        VERBATIM)
endif()

if(ANDROID)
    math(EXPR MIRU_VERSION_CODE "${PROJECT_VERSION_MAJOR} * 10000 + ${PROJECT_VERSION_MINOR} * 100 + ${PROJECT_VERSION_PATCH}")
    set_target_properties(Miru PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_PACKAGE_NAME "com.miru.app"
        QT_ANDROID_APP_NAME "Miru"
        QT_ANDROID_APP_ICON "@drawable/ic_launcher"
        QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
        # 1.1.0 -> 10100: grows with every release, as Android requires for updates.
        QT_ANDROID_VERSION_CODE ${MIRU_VERSION_CODE}
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION 36
    )
endif()

include(GNUInstallDirs)
install(TARGETS Miru
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

option(BUILD_TESTING "Build unit tests" ON)
if(BUILD_TESTING AND NOT EMSCRIPTEN AND NOT ANDROID)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test Pdf)
    qt_add_executable(tst_export tests/tst_export.cpp)
    target_link_libraries(tst_export PRIVATE boardcore collab Qt6::Test Qt6::CorePrivate Qt6::Pdf Qt6::WebSockets)
    add_test(NAME tst_export COMMAND tst_export)
endif()

# Standalone relay for live collaboration (desktop / server only).
if(NOT EMSCRIPTEN AND NOT ANDROID)
    qt_add_executable(MiruRelay tools/relay/main.cpp)
    target_link_libraries(MiruRelay PRIVATE collab)
    set_target_properties(MiruRelay PROPERTIES WIN32_EXECUTABLE OFF)
endif()
