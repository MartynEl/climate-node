cmake_minimum_required(VERSION 3.22)

project(climate_node C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(CLIMATE_BUILD_HOST_TESTS "Build host unit tests" ON)
option(CLIMATE_BUILD_HOST_RUNNER "Build host runner" ON)
option(CLIMATE_BUILD_MCU "Build MCU firmware" OFF)
option(CLIMATE_WERROR "Treat warnings as errors" ON)

if(CLIMATE_BUILD_MCU AND (CLIMATE_BUILD_HOST_TESTS OR CLIMATE_BUILD_HOST_RUNNER))
    message(FATAL_ERROR
        "MCU build must be configured in a separate build directory with "
        "-DCLIMATE_BUILD_HOST_TESTS=OFF -DCLIMATE_BUILD_HOST_RUNNER=OFF")
endif()

if(CLIMATE_BUILD_MCU AND NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "MCU build requires a cross toolchain. "
        "Use -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake")
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

add_custom_command(
    OUTPUT ${GENERATED_DIR}/dewpoint_lut.h
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_dewpoint_lut.py
            ${GENERATED_DIR}/dewpoint_lut.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_dewpoint_lut.py
    COMMENT "Generating dew point lookup table"
)

add_custom_target(climate_generate_lut
    DEPENDS ${GENERATED_DIR}/dewpoint_lut.h
)

add_library(climate_core STATIC
    src/core/errors.c
    src/core/filter.c
    src/core/dewpoint.c
    src/core/controller.c
    src/core/diagnostics.c
    src/core/platform_stubs.c
    src/core/i2c_stubs.c
    src/service/modbus.c
    src/service/modbus_server.c
    src/service/modbus_regs.c
    src/service/storage.c
)

add_dependencies(climate_core climate_generate_lut)

target_include_directories(climate_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${GENERATED_DIR}
)

target_compile_options(climate_core PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wdouble-promotion
    -Wformat=2
)

if(CLIMATE_WERROR)
    target_compile_options(climate_core PRIVATE -Werror)
endif()

if(CLIMATE_BUILD_HOST_RUNNER)
    add_executable(climate_host
        src/app/main_host.c
        src/app/app.c
        src/app/scheduler.c
        src/driver/mock_sensor.c
        src/platform/host/platform_host.c
        src/service/logger.c
    )

    target_link_libraries(climate_host PRIVATE climate_core)

    target_include_directories(climate_host PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_compile_options(climate_host PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wdouble-promotion
        -Wformat=2
    )

    if(CLIMATE_WERROR)
        target_compile_options(climate_host PRIVATE -Werror)
    endif()
endif()

if(CLIMATE_BUILD_HOST_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(CLIMATE_BUILD_MCU)
    set_source_files_properties(
        src/bsp/stm32f1/startup_stm32f103.c
        PROPERTIES COMPILE_OPTIONS "-Wno-pedantic;-Wno-error"
    )

    set_source_files_properties(
        src/platform/stm32f1/platform_stm32f1.c
        PROPERTIES COMPILE_OPTIONS "-Wno-pedantic"
    )

    set_source_files_properties(
        src/bsp/stm32f1/uart.c
        PROPERTIES COMPILE_OPTIONS "-Wno-pedantic"
    )

    add_executable(climate_fw
        src/app/main_mcu.c
        src/app/app.c
        src/app/scheduler.c
        src/driver/stub_sensor.c
        src/driver/sensor_sht31.c      # <-- НОВОЕ
        src/platform/stm32f1/platform_stm32f1.c
        src/bsp/stm32f1/startup_stm32f103.c
        src/bsp/stm32f1/uart.c
        src/service/logger.c
    )

    target_link_libraries(climate_fw PRIVATE climate_core)

    target_include_directories(climate_fw PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_compile_options(climate_fw PRIVATE
        -Wall
        -Wextra
        -Wshadow
        -Wdouble-promotion
        -Wformat=2
    )

    if(CLIMATE_WERROR)
        target_compile_options(climate_fw PRIVATE -Werror)
    endif()

    target_link_options(climate_fw PRIVATE
        -T ${CMAKE_CURRENT_SOURCE_DIR}/src/bsp/stm32f1/stm32f103c8tx.ld
        -Wl,--gc-sections
        -Wl,-Map=climate_fw.map
        -Wl,--print-memory-usage
    )

    add_custom_command(TARGET climate_fw POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:climate_fw> ${CMAKE_CURRENT_BINARY_DIR}/climate_fw.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:climate_fw> ${CMAKE_CURRENT_BINARY_DIR}/climate_fw.bin
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:climate_fw>
        VERBATIM
    )
endif()
