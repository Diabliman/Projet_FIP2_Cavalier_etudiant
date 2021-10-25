cmake_minimum_required(VERSION 3.16)
project(Projet_FIP2_Cavalier_etudiant C)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(SOURCE_FILES main.cpp)

add_executable(cavalier_GUI cavalier_GUI.c)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})

add_definitions(${GTK3_CFLAGS_OTHER})

target_link_libraries(cavalier_GUI ${GTK3_LIBRARIES})
target_link_libraries(cavalier_GUI Threads::Threads)

file(COPY UI_Glade DESTINATION "${CMAKE_BINARY_DIR}")

