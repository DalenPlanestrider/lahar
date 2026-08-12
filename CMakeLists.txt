cmake_minimum_required(VERSION 3.22)
project(lahar)

#add_compile_options(-fno-omit-frame-pointer -g)

#add_compile_options(-fsanitize=address)
#add_link_options(-fsanitize=address)

add_executable(lahar main.c)
target_link_libraries(lahar glfw)
target_include_directories(lahar SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(lahar PRIVATE -Wall -Wextra -Werror 
    -Wconversion -Wsign-conversion -Wshadow -Wformat=2
    -Wdouble-promotion -Wundef -Wcast-align -Wnull-dereference 
    -Wwrite-strings -Wfloat-equal -Wpointer-arith -Wredundant-decls
    -Wno-format-nonliteral -Wno-unused-parameter
)

add_executable(tests ${CMAKE_CURRENT_SOURCE_DIR}/test/tests.c)
target_include_directories(tests SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(cycle_tests ${CMAKE_CURRENT_SOURCE_DIR}/test/cycle.c)
target_link_libraries(cycle_tests glfw)
target_include_directories(cycle_tests SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(allocator_tests test/allocator.cpp)
target_include_directories(allocator_tests SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} test)
target_link_libraries(allocator_tests dl)
