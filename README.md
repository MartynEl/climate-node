# climate-node

Testable embedded firmware architecture for a climate control node.

This repository intentionally starts with a host-testable core:

- fixed-point temperature/humidity representation;
- moving-average filter with explicit warm-up state;
- Magnus-formula dew point calculation using integer Q10 math and LUT;
- relay control with hysteresis;
- sensor fault debouncing;
- safe-state output behavior;
- no dependency on STM32 HAL, CubeIDE, QEMU or real hardware in the core layer.

The long-term target is STM32, with possible future ports to RISC-V platforms.

## Current scope

The current commit contains the portable core and host unit tests.

It does not yet contain:

- STM32 BSP;
- UART driver;
- Modbus RTU protocol;
- flash storage;
- watchdog integration;
- hardware-in-the-loop tests.

These layers will be added above the stable core.

## Build and test on host

Requirements:

- CMake >= 3.22
- C11 compiler
- Python 3

```bash
cmake -B build -DCLIMATE_BUILD_HOST_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure