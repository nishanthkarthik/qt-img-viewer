cmake_minimum_required(VERSION 3.28)
project(typst_svg_viewer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)


find_package(Qt6 COMPONENTS
        Core
        Widgets
        SvgWidgets
        REQUIRED)

add_executable(typst_svg_viewer main.cpp)
target_link_libraries(typst_svg_viewer
        Qt::Core
        Qt::Widgets
        Qt::SvgWidgets
)

