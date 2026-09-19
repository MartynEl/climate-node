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

add_executable(test_scheduler
    test_scheduler.c
    ${CMAKE_SOURCE_DIR}/src/app/scheduler.c
)

target_link_libraries(test_scheduler PRIVATE climate_core)

target_include_directories(test_scheduler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(test_scheduler PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wdouble-promotion
    -Wformat=2
)

if(CLIMATE_WERROR)
    target_compile_options(test_scheduler PRIVATE -Werror)
endif()

add_test(NAME scheduler COMMAND test_scheduler)

add_executable(test_diagnostics
    test_diagnostics.c
)

target_link_libraries(test_diagnostics PRIVATE climate_core)

target_include_directories(test_diagnostics PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(test_diagnostics PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wdouble-promotion
    -Wformat=2
)

if(CLIMATE_WERROR)
    target_compile_options(test_diagnostics PRIVATE -Werror)
endif()

add_test(NAME diagnostics COMMAND test_diagnostics)