cmake_minimum_required(VERSION 3.28)
project(qt-img-viewer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS
        Core
        Widgets
        REQUIRED)

add_executable(img-viewer main.cpp)
target_link_libraries(img-viewer
        Qt::Core
        Qt::Widgets
)
