# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

Bare-metal firmware template for an STM32F429 (Cortex-M4) target, intended to integrate the
STM32F4xx HAL and Eclipse ThreadX (RTOS), built with CMake + Ninja and an `arm-none-eabi` GCC
toolchain. The project is in an early scaffolding stage: the CMake module graph is laid out but
`app/`, `bsp/`, and `middlewares/threadx/` currently contain only a `CMakeLists.txt` each with no
sources yet (no `src/`/`inc/` under `app` or `bsp`, no ThreadX kernel/port sources under
`middlewares/threadx`). Only `core/` and `drivers/` (STM32F4xx HAL/LL + CMSIS, vendored) have real
source files.

## Build

```bash
cmake -B build -G Ninja
cmake --build build
```

Post-build, the executable target (`${CMAKE_PROJECT_NAME}` = `stm32f429_firmware`) is converted to
`.hex`/`.bin` and its size is printed via `arm-none-eabi-objcopy`/`size`.

There is no test suite, linter, or flashing/debug tooling configured in this repo yet.

### Build status

As of 2026-09-10, verified against a real `arm-none-eabi-gcc` 15.3.1 + CMake 4.4.3 + Ninja 1.13.2
toolchain:

- All prior naming/typo bugs are fixed (mismatched `stm32_muc_flags`/`stm32_mcu_flags`,
  `threasx`/`threadx`, `bap`/`bsp`, `Src`/`Inc` case mismatches, `middleeares` typo,
  `arm-none-eabe-` prefix typo, malformed `target_compile_options`/`COMMAND` calls, wrong
  linker-script filename `STM32F429IGTx_FLASH.ld` → `STM32F429IGT6_FLASH.ld`).
- `drivers/CMakeLists.txt` no longer reaches into `../core/src/` for the startup/system files
  (that caused `system_stm32f4xx.c` to be compiled twice once `core`'s glob casing was corrected).
  `core` now owns `startup_stm32f429xx.S` and `system_stm32f4xx.c` via its own glob
  (`src/*.c` + `src/*.S`); `drivers` only builds the HAL sources.
- `drivers` needed `${CMAKE_SOURCE_DIR}/core/inc` added to its `target_include_directories` — HAL
  sources `#include "stm32f4xx_hal.h"` which needs the project's `stm32f4xx_hal_conf.h`, which
  (per the usual STM32CubeMX layout) lives in `core/inc` next to `main.h`, not in `drivers/inc`.
  This is a header search path only, not a CMake target link dependency (no `core → drivers`
  cycle).
- `app/`, `bsp/`, and `middlewares/threadx` `CMakeLists.txt` each guard their `file(GLOB ...)` with
  `if(NOT <SOURCES>)` and fall back to writing an empty stub `.c` into the build dir — modern CMake
  refuses `add_library(... STATIC)`/`add_executable(...)` with zero sources, and these three
  modules currently have none. The stub is skipped automatically once real sources exist. The root
  executable target has the same problem for the same reason (all its code comes from linked
  static libs) and gets a generated `exe_stub.c` the same way, plus an explicit
  `LINKER_LANGUAGE C` since a sourceless target can't infer one.

**Resolved**: `drivers/inc/` never got the `Legacy/` subfolder from the source STM32Cube_FW_F4
package (HAL tag v1.28.3), so `drivers/inc/stm32f4xx_hal_def.h`'s
`#include "Legacy/stm32_hal_legacy.h"` couldn't resolve. Per-project decision: rather than
vendoring that file, the include was dropped from `stm32f4xx_hal_def.h`. Grepping confirmed
`stm32f4xx_hal_eth.c` (Ethernet MAC driver) was the *only* HAL source relying on macros from that
header (`PHY_READ_TO`/`PHY_WRITE_TO`); it's excluded from `drivers/CMakeLists.txt`'s source list
via `list(REMOVE_ITEM ...)` since Ethernet isn't currently used. To re-enable Ethernet: drop
`list(REMOVE_ITEM HAL_SOURCES ".../stm32f4xx_hal_eth.c")` from `drivers/CMakeLists.txt` and either
restore the real `Legacy/stm32_hal_legacy.h` or define `PHY_READ_TO`/`PHY_WRITE_TO` yourself. Full
`cmake -B build -G Ninja && cmake --build build` now succeeds end-to-end and produces
`stm32f429_firmware.elf/.hex/.bin` (currently just HAL + empty `app`/`bsp`/`threadx` stub libs).

Toolchain note: `arm-none-eabi-gcc` on this dev machine is unpacked at a custom path
(`~/home/tools/arm-gnu-toolchain-*/bin`), not on `PATH` by default — add it to your shell profile
(`~/.zshrc`) rather than relying on ad hoc `export PATH=...` per session. `cmake`/`ninja` are
installed via Homebrew.

## Architecture

CMake module graph (each is a STATIC library except the root executable and the
`stm32_mcu_flags` INTERFACE library):

```
core        -> drivers, threadx, stm32_mcu_flags   (also owns startup_*.S and system_stm32f4xx.c)
drivers     -> stm32_mcu_flags
bsp         -> drivers, stm32_mcu_flags
threadx     -> stm32_mcu_flags
app         -> bsp, threadx, stm32_mcu_flags
```

The root `CMakeLists.txt` links the final `stm32f429_firmware` executable against
`app bsp threadx drivers core`. Toolchain flags (`-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16
-mfloat-abi=hard`) and the linker script (`cmake/STM32F429IGT6_FLASH.ld`) are centralized in the
`stm32_mcu_flags` INTERFACE library and propagated to every module through it, rather than being
repeated per-target.

Layer responsibilities (intended, per the module layout):
- `drivers/` — vendored STM32F4xx HAL/LL drivers and CMSIS headers (`cminc/`); not meant to be
  hand-edited except for local fixes.
- `core/` — CMSIS device startup/system files and `main.c`/`stm32f4xx_it.c`/HAL MSP config —
  the vendor "Templates" style entry point and IRQ handlers.
- `bsp/` — board support layer, sits on top of `drivers` (currently empty, no sources).
- `middlewares/threadx/` — intended to hold the ThreadX kernel (`common/`) and Cortex-M4 GNU port
  (`ports/cortex_m4/gnu/`) sources, referencing `tx_user.h` from `core/inc` (currently empty).
- `app/` — application/business logic, depends on `bsp` and `threadx` (currently empty).

Toolchain file `cmake/arm-none-eabi.cmake` sets `CMAKE_SYSTEM_NAME Generic` and
`CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY` (required for cross-compiling bare-metal — the
toolchain can't link a runnable test executable), and gives compiled artifacts a `.elf` suffix.
