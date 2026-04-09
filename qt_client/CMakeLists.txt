cmake_minimum_required(VERSION 3.16)
project(PoseQtClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(PoseQtClient
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
)

target_link_libraries(PoseQtClient PRIVATE Qt6::Widgets)
