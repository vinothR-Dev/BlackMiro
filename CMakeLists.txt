cmake_minimum_required(VERSION 3.21)

project(BlackMiro VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 QuickTemplates2 Svg Network WebSockets)
# QZipReader/QZipWriter live in QtCore's private API (stable for many years).
find_package(Qt6 REQUIRED COMPONENTS CorePrivate)

qt_standard_project_setup(REQUIRES 6.8)

add_subdirectory(src/core)
add_subdirectory(src/collab)

# BlackMiro's own Qt Quick Controls style (rounded fields, buttons, menus, dialogs).
# Colours come from the window palette, which follows the light / dark theme.
qt_add_library(blackmirostyle STATIC)
qt_add_qml_module(blackmirostyle
    URI BlackMiroStyle
    VERSION 1.0
    IMPORTS QtQuick.Controls.Basic
    QML_FILES
        style/BlackMiroStyle/Button.qml
        style/BlackMiroStyle/ToolButton.qml
        style/BlackMiroStyle/TextField.qml
        style/BlackMiroStyle/TextArea.qml
        style/BlackMiroStyle/CheckBox.qml
        style/BlackMiroStyle/ComboBox.qml
        style/BlackMiroStyle/SpinBox.qml
        style/BlackMiroStyle/Slider.qml
        style/BlackMiroStyle/Dialog.qml
        style/BlackMiroStyle/DialogButtonBox.qml
        style/BlackMiroStyle/Popup.qml
        style/BlackMiroStyle/Menu.qml
        style/BlackMiroStyle/MenuItem.qml
        style/BlackMiroStyle/MenuSeparator.qml
        style/BlackMiroStyle/ToolTip.qml
)
target_link_libraries(blackmirostyle PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::QuickTemplates2)

qt_add_executable(BlackMiro
    src/app/main.cpp
)

set_source_files_properties(qml/Theme.qml qml/ToolRegistry.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(BlackMiro
    URI BlackMiro
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
        qml/ToolsPanel.qml qml/CommentPopup.qml qml/LinkDialog.qml qml/SavedFilesDialog.qml qml/ShortcutsDialog.qml qml/CursorChat.qml qml/ConnectorToolbar.qml
    SOURCES
        src/app/BoardCanvas.h src/app/BoardCanvas.cpp src/app/BoardCanvasExtras.cpp src/app/BoardCanvasMenus.cpp
        src/app/ExportController.h src/app/ExportController.cpp
        src/app/AppState.h src/app/AppState.cpp
        src/app/CollabController.h src/app/CollabController.cpp
        src/app/TemplatePreviewProvider.h
        src/app/LibraryPreviewProvider.h
)

target_link_libraries(BlackMiro PRIVATE
    boardcore
    blackmirostyleplugin
    collab
    Qt6::WebSockets
    Qt6::Network
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Svg
)

target_include_directories(BlackMiro PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/app)

# Bundled colour-emoji font, used only where the system has none (e.g. the
# browser build). Drop a COLR emoji .ttf at fonts/emoji.ttf to enable it.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/fonts/emoji.ttf)
    qt_add_resources(BlackMiro "emojifont" PREFIX "/fonts" BASE fonts FILES fonts/emoji.ttf)
endif()

target_compile_definitions(BlackMiro PRIVATE PROJECT_VERSION="${PROJECT_VERSION}")

set_target_properties(BlackMiro PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

if(EMSCRIPTEN)
    # Browser open/download dialogs (QWasmLocalFileAccess) live in QtGui's private API.
    find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
    target_link_libraries(BlackMiro PRIVATE Qt6::GuiPrivate)
endif()

if(ANDROID)
    set_target_properties(BlackMiro PROPERTIES
        QT_ANDROID_PACKAGE_NAME "org.blackmiro.app"
        QT_ANDROID_APP_NAME "BlackMiro"
        QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
        QT_ANDROID_VERSION_CODE 1
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION 36
    )
endif()

include(GNUInstallDirs)
install(TARGETS BlackMiro
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
    qt_add_executable(BlackMiroRelay tools/relay/main.cpp)
    target_link_libraries(BlackMiroRelay PRIVATE collab)
    set_target_properties(BlackMiroRelay PROPERTIES WIN32_EXECUTABLE OFF)
endif()
