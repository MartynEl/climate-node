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
        src/driver/mock_sensor.c
        src/platform/host/platform_host.c
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