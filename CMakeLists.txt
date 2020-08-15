cmake_minimum_required(VERSION 3.10)
project(lc3sim-c
	DESCRIPTION "lc3sim-c project"
)

set(INSTALL_DIR "${CMAKE_BINARY_DIR}/bin")

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

ADD_CUSTOM_COMMAND(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lc3os.c
		SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/lc3os.obj
		COMMAND xxd -i lc3os.obj > ${CMAKE_CURRENT_BINARY_DIR}/lc3os.c
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMENT "Generating lc3os.c"
	)

set(SOURCES
	main.c
	${CMAKE_CURRENT_BINARY_DIR}/lc3os.c
	vm.h
	vm.c
	)

set(CMAKE_C_COMPILER gcc)
add_executable(lc3sim-c
	${SOURCES}
	)
target_compile_options(lc3sim-c PRIVATE -O2 -Wall -g --std=c11)
set_target_properties(lc3sim-c PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${INSTALL_DIR}/")
