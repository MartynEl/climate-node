# Climate Node: Industrial Embedded Firmware Prototype

[![CI Status](https://img.shields.io/badge/build-passing-brightgreen)](#)
[![License](https://img.shields.io/badge/license-MIT-blue)](LICENSE)
[![Tests](https://img.shields.io/badge/tests-10%2F10-green)](#build-and-test-on-host)

A testable, bare-metal-style firmware architecture for an industrial climate control node targeting STM32 (ARM Cortex-M3). Designed with strict portability constraints to allow future migration to RISC-V platforms (e.g., CH32V, MIK32 "Amur").

This project demonstrates senior-level embedded engineering practices: fixed-point arithmetic, explicit state machines, fault-tolerant communication protocols (Modbus RTU), persistent configuration storage, and a layered architecture decoupled from hardware drivers via Weak Symbols.

## 🎯 Key Features

*   **Portable Core:** Business logic (`src/core`) has zero dependencies on HAL, CMSIS, or specific microcontroller registers. It is fully unit-testable on any PC using standard GCC.
*   **Fixed-Point Math:** All temperature/humidity calculations use integer Q-format (0.01°C / 0.01%) to avoid floating-point overhead and ensure deterministic timing on MCUs without FPU.
*   **Industrial Protocols:** Native Modbus RTU slave implementation with CRC validation, exception handling, and register mapping for telemetry and configuration.
*   **Reliability First:**
    *   Explicit warm-up states (filter initialization).
    *   Sensor fault debouncing (N consecutive errors required to trigger alarm).
    *   Hysteresis-based relay control to prevent chattering.
    *   Safe-state outputs upon sensor failure.
    *   Independent Watchdog (IWDG) integration with ticket-based feeding.
*   **Persistent Configuration:** Device settings are stored in Flash with CRC32 validation and A/B slot fallback mechanism.
*   **Test Driven:** 10+ host unit tests covering filter logic, dew point calculation, protocol parsing, storage integrity, and state machine transitions.

## 🏗️ Architecture Overview

The project follows a strict layered architecture to ensure separation of concerns and testability.

### Layer Diagram (Textual)

```text
+---------------------+
|   Application Layer |  (Scheduler, Main Loop, Task Glue)
+----------+----------+
           |
           v
+---------------------+
|     Core Logic      |  (Platform Independent C Code)
| - Controller FSM    |  <--- Fixed Point Math
| - Diagnostics       |  <--- Error Counters & Uptime
| - Modbus Server     |  <--- Protocol Parsing & Response
| - Storage Service   |  <--- Config Persistence Logic
+----------+----------+
           |
           v
+---------------------+
|      Services       |  (Logger Ring Buffer, Utilities)
+----------+----------+
           |
           v
+---------------------+
|   BSP / Drivers     |  (Hardware Specific Implementations)
| - STM32 UART TX/RX  |
| - STM32 IWDG        |
| - SHT31 I2C Driver  |
| - Flash Write/Read  |
+----------+----------+
           |
           v
+---------------------+
|   Hardware Target   |  (STM32F103 / Future RISC-V)
+---------------------+
```

### Why this structure?

1.  **Unit Testability:** The `Core` layer is isolated. You can run `ctest` on your laptop to verify the physics engine and protocol logic without flashing a chip.
2.  **Portability:** Switching from STM32 to RISC-V only requires implementing new BSP drivers (`src/platform/riscv`) and overriding weak platform symbols. The business logic remains untouched.
3.  **Safety Critical Mindset:** Faults are handled explicitly (debounced errors, safe states, watchdog tickets) rather than ignored or hidden behind blocking delays.
4.  **Decoupling:** Hardware interfaces are abstracted via headers (`include/platform`, `include/bsp`). The core never includes `<stm32f1xx.h>` directly.

## 🛠️ Build Instructions

### Prerequisites

*   CMake >= 3.22
*   GCC (for host tests)
*   ARM Cross Toolchain (`arm-none-eabi-gcc`) for MCU build
*   Python 3 (for LUT generation during build)

### Host Tests & Simulation

Run logic verification without hardware. This builds the portable core and executes all unit tests.

```bash
# Configure for host testing
cmake -B build -DCLIMATE_BUILD_HOST_TESTS=ON -DCLIMATE_BUILD_HOST_RUNNER=ON

# Build
cmake --build build -j

# Run Unit Tests
ctest --test-dir build --output-on-failure

# Run Interactive Simulator (prints logs to stdout)
./build/climate_host
```
Expected output from simulator:

```text
[BOOT] climate-node host runner started
[000050] state=WARMUP err=WARMUP T=22.23 RH=65.00 DP=0.00 RELAY=0
...
[000200] state=RUN err=OK T=22.51 RH=65.00 DP=15.60 RELAY=0
...
[DONE] uptime=3000 samples=61 fault_events=1 sensor_errors=5 warmups=4
```
### STM32 Firmware Build
Generate .elf, .hex, and .bin files for flashing.

```bash
# Clean previous MCU build if exists
rm -rf build-mcu

# Configure for MCU cross-compilation
cmake -B build-mcu \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
  -DCLIMATE_BUILD_MCU=ON \
  -DCLIMATE_BUILD_HOST_TESTS=OFF \
  -DCLIMATE_BUILD_HOST_RUNNER=OFF

# Build
cmake --build build-mcu -j
```

Output artifacts will be in `build-mcu/`:
*   `climate_fw.elf` (Debuggable ELF)
*   `climate_fw.hex` (Intel HEX for flashing)
*   `climate_fw.bin` (Raw binary)
*   `climate_fw.map` (Memory map)

## 🔌 Hardware Integration Guide

To run on real hardware (e.g., Blue Pill STM32F103C8T6):

| Component | Pin Assignment | Notes |
|-----------|----------------|-------|
| LED (Status) | PC13 | Active Low (Onboard LED) |
| UART TX | PA9 | 115200 baud, 8N1 (Console/Modbus) |
| UART RX | PA10 | For Modbus commands |
| I2C SDA | PB7 | Connect SHT31 sensor (or compatible) |
| I2C SCL | PB6 | Connect SHT31 sensor |
| SWDIO/SWCLK | PA13/PA14 | Programming interface |

### Flashing

Using OpenOCD with ST-Link V2:

```bash
openocd -f interface/stlink.cfg \
        -f target/stm32f1x.cfg \
        -c "program build-mcu/climate_fw.hex verify reset exit"
```
Or using STM32CubeProgrammer GUI.

## 📡 Modbus Register Map

The device acts as a Modbus RTU Slave (Default Address: `0x01`).

| Address | Name | Type | Access | Description |
|---------|------|------|--------|-------------|
| `0x0000` | Temperature | int16 | RO | Filtered temp in 0.01 °C |
| `0x0001` | Humidity | uint16 | RO | Relative humidity in 0.01 % |
| `0x0002` | Dew Point | int16 | RO | Calculated dew point in 0.01 °C |
| `0x0003` | Relay State | uint16 | RO | Heater relay status (0=Off, 1=On) |
| `0x0004` | Device Status | uint16 | RO | Current controller state enum |
| `0x0005` | Error Count | uint16 | RO | Cumulative fault events since boot |
| `0x0006` | Uptime Low | uint16 | RO | Lower 16 bits of uptime_ms |
| `0x0007` | Uptime High | uint16 | RO | Upper 16 bits of uptime_ms |
| `0x0100` | Margin ON | int16 | RW | Hysteresis turn-on threshold (ΔT) |
| `0x0101` | Margin OFF | int16 | RW | Hysteresis turn-off threshold (ΔT) |
| `0x0102` | Sample Period | uint16 | RW | Sensor polling period in ms |

Supported Functions:
*   `0x03` Read Holding Registers
*   `0x06` Write Single Register

## ⚠️ Known Limitations & Roadmap

*   **No DMA yet:** UART/I2C use polling/interrupts. Suitable for low-speed comms but blocks CPU under high load.
*   **Single Instance Sensors:** Currently supports one global I2C bus instance. Multi-sensor support planned.
*   **Bootloader Missing:** OTA updates not implemented. Memory map reserves space for future bootloader.
*   **Mock I2C on MCU:** The current STM32 I2C driver returns dummy data. Real hardware bring-up requires validating timing against datasheet.

## License

MIT License - see [LICENSE](LICENSE) file.
