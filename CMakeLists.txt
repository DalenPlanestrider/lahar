cmake_minimum_required(VERSION 3.22)
project(lahar)

add_executable(lahar main.c)
target_link_libraries(lahar glfw)
target_include_directories(lahar SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})