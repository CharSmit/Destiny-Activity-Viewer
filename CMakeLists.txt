cmake_minimum_required(VERSION 3.16)
project(CsvViewer)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_PREFIX_PATH "/opt/homebrew/opt/qt")
find_package(Qt6 REQUIRED COMPONENTS Widgets)
include_directories(/opt/homebrew/opt/curl/include)
link_directories(/opt/homebrew/opt/curl/lib)

qt_add_executable(CsvViewer
    main.cpp
    viewer.cpp
    viewerWindow.h
    tracker.cpp
)

target_link_libraries(CsvViewer
    PRIVATE
    Qt6::Widgets
    curl
)