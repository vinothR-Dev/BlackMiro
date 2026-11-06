qt_add_library(boardcore STATIC
    BoardItem.h BoardItem.cpp
    Document.h Document.cpp
    UndoStack.h UndoStack.cpp
    Renderer.h Renderer.cpp
    BoardFile.h BoardFile.cpp
    Exporter.h Exporter.cpp
    SvgWriter.h SvgWriter.cpp
    Crypto.h Crypto.cpp
    PdfEncrypt.h PdfEncrypt.cpp
    Templates.h Templates.cpp
    SyncEngine.h SyncEngine.cpp
    SvgPath.h SvgPath.cpp
    ShapeLibrary.h ShapeLibrary.cpp
    PenTools.h PenTools.cpp
)

target_include_directories(boardcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(boardcore
    PUBLIC Qt6::Core Qt6::Gui
    PRIVATE Qt6::CorePrivate
)
