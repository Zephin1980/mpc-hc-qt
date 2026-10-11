cmake_minimum_required(VERSION 3.16)
project(mpc-hc-linux VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)

add_executable(mpc-hc
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/MpvBackend.cpp
    src/MpvBackend.h
    src/PlayerBackend.h
    src/SeekBar.cpp
    src/SeekBar.h
    src/Commands.h
)

target_link_libraries(mpc-hc PRIVATE Qt6::Widgets Qt6::OpenGLWidgets PkgConfig::MPV)

install(TARGETS mpc-hc RUNTIME DESTINATION bin)
