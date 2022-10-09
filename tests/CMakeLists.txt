add_test(lc3sim_hello-world
    ../lc3sim ${CMAKE_CURRENT_SOURCE_DIR}/hello-world/hello-world.obj
)

set_tests_properties(lc3sim_hello-world PROPERTIES
    PASS_REGULAR_EXPRESSION "hello world!\n\n--- halting the LC-3 ---\n\n"
)
