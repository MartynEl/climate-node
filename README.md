# Climate Node: Modular Embedded Firmware Prototype

[![CI Status](https://img.shields.io/badge/build-passing-brightgreen)](#)
[![License](https://img.shields.io/badge/license-MIT-blue)](LICENSE)
[![Tests](https://img.shields.io/badge/tests-10%2F10-green)](#build-and-test-on-host)

A testable, layered firmware architecture for an industrial climate control node targeting STM32 (ARM Cortex-M3). 

This project demonstrates **software engineering rigor applied to embedded systems**: strict separation of concerns, fixed-point arithmetic for deterministic timing, native Modbus RTU implementation, and comprehensive host-side unit testing without hardware dependency.

## 🎯 Project Goal & Scope

The primary goal is to prove that complex embedded logic can be developed, tested, and maintained using modern software engineering practices (TDD, CI/CD, Dependency Injection), even by engineers transitioning from high-level backend development.

### ✅ Implemented (Production-Ready Logic)
*   **Portable Core (`src/core`):** 
    *   Finite State Machine (FSM) for climate control with hysteresis and fault debouncing.
    *   Fixed-point math engine (Q-format) for dew point calculation (Magnus-Tetens approximation).
    *   Moving Average filter for sensor noise suppression.
    *   Diagnostic counters and uptime tracking.
*   **Communication Services (`src/service`):**
    *   Native Modbus RTU Slave parser (Func 0x03, 0x06) with CRC16 validation.
    *   Register mapping layer decoupled from transport.
    *   Persistent configuration storage logic with CRC32 integrity checks.
*   **Application Layer (`src/app`):**
    *   Cooperative scheduler with ticket-based watchdog supervision.
    *   Composition root wiring drivers, core, and services via interfaces.
*   **Testing Infrastructure:**
    *   10+ host-side unit tests covering all core modules.
    *   Mock sensors and flash emulators for hardware-free verification.
    *   GitHub Actions CI pipeline for automated build and test execution.

### ⚠️ Stubs & Mocks (Hardware Bring-Up Pending)
*   **STM32 BSP (`src/bsp/stm32f1`):**
    *   UART driver includes RS-485 direction control logic but lacks real-world signal integrity tuning.
    *   I2C driver is a skeleton returning dummy data; requires register-level implementation against SHT31 datasheet.
    *   Flash write operations are simulated; actual page erase/program sequences need validation on target silicon.
*   **Sensor Driver:**
    *   Current MCU entry point uses `stub_sensor_read`. Real SHT31 integration awaits physical hardware access.

### 🚀 Roadmap
1.  **Hardware Validation:** Purchase Blue Pill + SHT31, implement real I2C bit-banging or LL driver, validate RS-485 timing on oscilloscope.
2.  **DMA Optimization:** Move UART RX/TX to DMA to reduce CPU load during communication bursts.
3.  **Bootloader:** Implement simple serial bootloader for OTA updates.
4.  **RISC-V Migration:** Prove portability by compiling core/services for CH32V series.

## 🏗️ Architecture Highlights

### Why this structure matters for Embedded Teams?
1.  **Testability First:** The `Core` layer has zero dependencies on HAL/CMSIS. You can run `ctest` on your laptop to verify physics engines and protocol parsers before flashing a chip. This reduces debugging cycles significantly.
2.  **Deterministic Math:** No floating-point libraries. All calculations use integer Q-format (0.01°C / 0.01%), ensuring predictable execution time on Cortex-M3 without FPU.
3.  **Fault Tolerance:** Explicit handling of sensor failures (debouncing), communication errors (CRC checks), and system hangs (watchdog tickets).
4.  **Backend-Inspired Patterns:** Uses concepts familiar to web developers—Dependency Injection (via function pointers/interfaces), DTOs (Data Transfer Objects like `app_report_t`), and clear layer boundaries.

## 🛠️ Build & Test Instructions

### Prerequisites
*   CMake >= 3.22
*   GCC (for host tests)
*   ARM Cross Toolchain (`arm-none-eabi-gcc`) for MCU build
*   Python 3 (for LUT generation)

### Host Tests (No Hardware Needed)
Verify logic correctness immediately:

[INSERT CODE BLOCK HERE: Bash commands for cmake -B build ... ctest --test-dir build ... ./build/climate_host]

### STM32 Firmware Build
Generate artifacts for flashing:

```bash
rm -rf build-mcu
cmake -B build-mcu \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
  -DCLIMATE_BUILD_MCU=ON \
  -DCLIMATE_BUILD_HOST_TESTS=OFF \
  -DCLIMATE_BUILD_HOST_RUNNER=OFF
cmake --build build-mcu -j
```

## 📡 Modbus Register Map

| Address | Name | Type | Access | Description |
|---------|------|------|--------|-------------|
| `0x0000` | Temperature | int16 | RO | Filtered temp in 0.01 °C |
| `0x0001` | Humidity | uint16 | RO | Relative humidity in 0.01 % |
| `0x0002` | Dew Point | int16 | RO | Calculated dew point in 0.01 °C |
| `0x0003` | Relay State | uint16 | RO | Heater relay status (0=Off, 1=On) |
| `0x0100` | Margin ON | int16 | RW | Hysteresis turn-on threshold |
| `0x0101` | Margin OFF | int16 | RW | Hysteresis turn-off threshold |

Supported Functions: `0x03` Read Holding Registers, `0x06` Write Single Register.

## 👤 About the Author

With over 35 years of experience in backend development (Node.js, C#, SQL, Algorithms), I am transitioning into embedded systems engineering. This project serves as my portfolio piece demonstrating how mature software engineering practices can elevate embedded firmware reliability and maintainability.

I bring strong skills in:
*   Architectural design and code review.
*   Automated testing and CI/CD pipelines.
*   Complex algorithm implementation (fixed-point math, state machines).
*   Documentation and knowledge sharing.

I am eager to apply these strengths to the challenges of resource-constrained environments while continuing to learn low-level hardware specifics daily.

## License

MIT License - see [LICENSE](LICENSE) file.
