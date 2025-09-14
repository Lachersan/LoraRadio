include(FetchContent)

cmake_minimum_required(VERSION 3.31)
project(LoraRadio)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH "F:/Qt/6.9.0/mingw_64")
set(CMAKE_DISABLE_FIND_PACKAGE_WrapVulkanHeaders ON)

# Настройка libVLC
set(VLC_ROOT "F:/vlc-3.0.21")

# Поиск заголовков (исправленный путь с /sdk/include)
find_path(VLC_INCLUDE_DIR
        NAMES vlc/libvlc.h
        PATHS "${VLC_ROOT}/sdk/include"
        NO_DEFAULT_PATH
)

# Поиск библиотек (.lib, исправленный путь с /sdk/lib)
find_library(VLC_LIBRARY
        NAMES libvlc
        PATHS "${VLC_ROOT}/sdk/lib"
        NO_DEFAULT_PATH
)

find_library(VLCCORE_LIBRARY
        NAMES libvlccore
        PATHS "${VLC_ROOT}/sdk/lib"
        NO_DEFAULT_PATH
)

# Проверка, что всё найдено
if(NOT VLC_INCLUDE_DIR)
    message(FATAL_ERROR "VLC include directory not found. Check ${VLC_ROOT}/sdk/include/vlc/libvlc.h")
endif()

if(NOT VLC_LIBRARY OR NOT VLCCORE_LIBRARY)
    message(FATAL_ERROR "VLC libraries not found. Check ${VLC_ROOT}/sdk/lib/libvlc.lib and libvlccore.lib")
endif()

message(STATUS "VLC found: ${VLC_INCLUDE_DIR}")
message(STATUS "VLC lib: ${VLC_LIBRARY}")
message(STATUS "VLCCore lib: ${VLCCORE_LIBRARY}")

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
        include/FontLoader.h
        include/IconUtils.h
        include/fluent_icons.h
        src/IconButton.cpp
        src/MainWindow.cpp
        src/MainWindow.h
        styles/style.qrc
        include/AbstractPlayer.h
        src/YTPlayer.cpp
        src/YTPlayer.h
        src/RadioPage.cpp
        src/RadioPage.h
        src/YouTubePage.cpp
        src/YouTubePage.h
        src/SwitchPlayer.cpp
        src/SwitchPlayer.h
)

# Инклуды для libVLC
target_include_directories(LoraRadio PRIVATE
        ${VLC_INCLUDE_DIR}
)

# Новая линковка с libVLC
target_link_libraries(LoraRadio
        ${VLC_LIBRARY}
        ${VLCCORE_LIBRARY}
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Multimedia
)

# Копирование DLL и plugins для libVLC (Windows, исправленные пути: DLL в root, plugins в root)
if(WIN32)
    # DLL (в корне VLC, не в sdk/lib)
    add_custom_command(TARGET LoraRadio POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VLC_ROOT}/libvlc.dll"
            "${VLC_ROOT}/libvlccore.dll"
            $<TARGET_FILE_DIR:LoraRadio>
            COMMENT "Copying VLC DLLs"
    )

    # Папка plugins (в корне VLC)
    add_custom_command(TARGET LoraRadio POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${VLC_ROOT}/plugins"
            $<TARGET_FILE_DIR:LoraRadio>/plugins
            COMMENT "Copying VLC plugins"
    )
endif()

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