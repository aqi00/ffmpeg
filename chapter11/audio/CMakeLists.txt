cmake_minimum_required(VERSION 3.5)

project(audio VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
# 寻找多媒体组件
find_package(Qt6 REQUIRED COMPONENTS Multimedia)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

# link_directories要放在add_executable之前
# 添加头文件的所在目录
include_directories(../ffmpeg/include ../sdl2/include ../lame/include)
# 指定lib文件的链接目录
link_directories(../ffmpeg/lib ../sdl2/lib ../lame/lib)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(audio
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        sdlplayer.h sdlplayer.cpp
        sinkplayer.h sinkplayer.cpp
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET audio APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(audio SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(audio
            ${PROJECT_SOURCES}
        )
    endif()
endif()

# 设置名叫ffmpeg-libs的库集合，指定它包括哪些so库文件
set(ffmpeg-libs avformat avcodec avfilter swresample swscale avutil SDL2 mp3lame)

# 指定要链接哪些库。ffmpeg-libs的设置见上。Multimedia表示链接多媒体库
#target_link_libraries(audio PRIVATE Qt${QT_VERSION_MAJOR}::Widgets ${ffmpeg-libs})
target_link_libraries(audio PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt6::Multimedia ${ffmpeg-libs})

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.audio)
endif()
set_target_properties(audio PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS audio
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(audio)
endif()
