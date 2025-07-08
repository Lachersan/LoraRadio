include(FetchContent)

cmake_minimum_required(VERSION 3.31)
project(LoraRadio)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH "F:/Qt/6.9.0/mingw_64")
set(CMAKE_DISABLE_FIND_PACKAGE_WrapVulkanHeaders ON)

include_directories(${CMAKE_SOURCE_DIR}/thirdparty/libmpv/include)

link_directories(${CMAKE_SOURCE_DIR}/thirdparty/libmpv)


find_package(Qt6 COMPONENTS
        Core
        Gui
        Widgets
        Multimedia
        REQUIRED)

qt_add_resources(QDARKSTYLE_RCC
        /styles/style.qrc
)


add_executable(LoraRadio WIN32
        src/main.cpp
        src/RadioPlayer.cpp
        src/RadioPlayer.h
        src/trayiconmanager.cpp
        src/trayiconmanager.h
        src/AutoStartRegistry.cpp
        src/AutoStartRegistry.h
        resources/resources.qrc
        fonts/fonts.qrc
        resources/image/app.rc
        src/stationmanager.cpp
        src/stationmanager.h
        src/StationDialog.cpp
        src/StationDialog.h
        src/StationDialog.ui
        src/QuickControlPopup.cpp
        src/QuickControlPopup.h
        src/IconButton.h
        iclude/FontLoader.h
        iclude/IconUtils.h
        iclude/fluent_icons.h
        src/IconButton.cpp
        src/MainWindow.cpp
        src/MainWindow.h
        styles/style.qrc

)
target_link_libraries(LoraRadio
        mpv
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Multimedia
)

add_custom_command(TARGET LoraRadio POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/thirdparty/libmpv/libmpv-2.dll
        $<TARGET_FILE_DIR:LoraRadio>
)



set(WINDEPLOYQT_PATH "F:/Qt/6.9.0/mingw_64/bin/windeployqt.exe")

set(TARGET_EXE "$<TARGET_FILE:LoraRadio>")

add_custom_command(TARGET LoraRadio POST_BUILD
        COMMAND "${WINDEPLOYQT_PATH}" "${TARGET_EXE}"
        COMMENT "Запуск windeployqt для копирования Qt DLL и плагинов"
)

if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(DEBUG_SUFFIX)
    if (MSVC AND CMAKE_BUILD_TYPE MATCHES "Debug")
        set(DEBUG_SUFFIX "d")
    endif ()
    set(QT_INSTALL_PATH "${CMAKE_PREFIX_PATH}")
    if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
        set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
            set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        endif ()
    endif ()
    if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
    endif ()
    foreach (QT_LIB Core Gui Widgets)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/bin/Qt6${QT_LIB}${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    endforeach (QT_LIB)
endif ()
