cmake_minimum_required(VERSION 3.16)

project(MiniAccess VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES



)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(MiniAccess
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        core/AvailList.cpp core/AvailList.h core/Page.cpp core/Page.h core/Record.cpp core/Record.h core/Schema.h core/Schema.cpp core/Storage.cpp core/Storage.h
        app/main.cpp
        core/Table.h core/Table.cpp
        core/indexstorage.h core/indexstorage.cpp
        core/bplustreeint32.h core/bplustreeint32.cpp
        core/indexint32.h core/indexint32.cpp
        core/bplustreestring.h core/bplustreestring.cpp
        core/indexstring.h core/indexstring.cpp
        gui/MainWindow.cpp gui/MainWindow.h
        gui/tablemodel.h gui/tablemodel.cpp
        gui/boolcheckdelegate.h gui/boolcheckdelegate.cpp
        gui/theme.h gui/theme.cpp
        resources.qrc
        gui/objectsdock.h gui/objectsdock.cpp
        gui/datasheetpage.h gui/datasheetpage.cpp
        gui/designpage.h gui/designpage.cpp
        gui/querybuilderpage.h gui/querybuilderpage.cpp
        gui/relationdesignerpage.h gui/relationdesignerpage.cpp


        gui/delegates/datetimedelegate.h gui/delegates/datetimedelegate.cpp
        gui/formatdelegate.h gui/formatdelegate.cpp
        core/DisplayFmt.h
        gui/querymodel.h gui/querymodel.cpp
        gui/newtabledialog.h gui/newtabledialog.cpp

    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET MiniAccess APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(MiniAccess SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(MiniAccess
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(MiniAccess PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.MiniAccess)
endif()
set_target_properties(MiniAccess PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS MiniAccess
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(MiniAccess)
endif()
