add_executable(test_filter
    test_filter.c
)

target_link_libraries(test_filter PRIVATE climate_core)

target_include_directories(test_filter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME filter COMMAND test_filter)


add_executable(test_dewpoint
    test_dewpoint.c
)

target_link_libraries(test_dewpoint PRIVATE climate_core)

target_include_directories(test_dewpoint PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME dewpoint COMMAND test_dewpoint)


add_executable(test_controller
    test_controller.c
)

target_link_libraries(test_controller PRIVATE climate_core)

target_include_directories(test_controller PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME controller COMMAND test_controller)