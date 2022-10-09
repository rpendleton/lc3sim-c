cmake_minimum_required(VERSION 3.21)
project(lc3sim-c)

set(CMAKE_C_COMPILER gcc)
set(CMAKE_C_STANDARD 17)

add_custom_command(
    COMMAND xxd -i lc3os.obj > ${CMAKE_CURRENT_BINARY_DIR}/lc3os.c
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/lc3os.obj
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lc3os.c
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(lc3sim
    main.c
    vm.h
    vm.c
    ${CMAKE_CURRENT_BINARY_DIR}/lc3os.c
)

target_compile_options(lc3sim PRIVATE -Wall -Wextra)
