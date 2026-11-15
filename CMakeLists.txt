cmake_minimum_required(VERSION 3.16)
project(SCAPMS VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

add_executable(scapms
    src/main.cpp
    src/model/PatientRecord.cpp
    src/model/ClinicDatabase.cpp
    src/controller/SearchEngine.cpp
    src/controller/ClinicManager.cpp
    src/io/FileHandler.cpp
    src/gui/Ui.cpp
    src/gui/Dialogs.cpp
    src/gui/SearchPanel.cpp
    src/gui/MainWindow.cpp
)
target_include_directories(scapms PRIVATE src)
target_link_libraries(scapms PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

if(MSVC)
    target_compile_options(scapms PRIVATE /W4 /utf-8)
else()
    target_compile_options(scapms PRIVATE -Wall -Wextra -Wpedantic)
endif()
