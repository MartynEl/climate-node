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
add_executable(test_modbus
    test_modbus.c
)

target_link_libraries(test_modbus PRIVATE climate_core)

target_include_directories(test_modbus PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(test_modbus PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wdouble-promotion
    -Wformat=2
)

if(CLIMATE_WERROR)
    target_compile_options(test_modbus PRIVATE -Werror)
endif()

add_test(NAME modbus COMMAND test_modbus)

add_executable(test_modbus_server
    test_modbus_server.c
)

target_link_libraries(test_modbus_server PRIVATE climate_core)

target_include_directories(test_modbus_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(test_modbus_server PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wdouble-promotion
    -Wformat=2
)

if(CLIMATE_WERROR)
    target_compile_options(test_modbus_server PRIVATE -Werror)
endif()

add_test(NAME modbus_server COMMAND test_modbus_server)

add_executable(test_modbus_regs
    test_modbus_regs.c
)

target_link_libraries(test_modbus_regs PRIVATE climate_core)

target_include_directories(test_modbus_regs PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(test_modbus_regs PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wdouble-promotion
    -Wformat=2
)

if(CLIMATE_WERROR)
    target_compile_options(test_modbus_regs PRIVATE -Werror)
endif()

add_test(NAME modbus_regs COMMAND test_modbus_regs)

add_executable(test_storage
    test_storage.c
)

target_link_libraries(test_storage PRIVATE climate_core)

target_include_directories(test_storage PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(test_storage PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wdouble-promotion
    -Wformat=2
)

if(CLIMATE_WERROR)
    target_compile_options(test_storage PRIVATE -Werror)
endif()

add_test(NAME storage COMMAND test_storage)

add_executable(test_sensor_sht31
    test_sensor_sht31.c
    ${CMAKE_SOURCE_DIR}/src/driver/sensor_sht31.c
)

target_link_libraries(test_sensor_sht31 PRIVATE climate_core)

target_include_directories(test_sensor_sht31 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(test_sensor_sht31 PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wdouble-promotion
    -Wformat=2
)

if(CLIMATE_WERROR)
    target_compile_options(test_sensor_sht31 PRIVATE -Werror)
endif()

add_test(NAME sensor_sht31 COMMAND test_sensor_sht31)
