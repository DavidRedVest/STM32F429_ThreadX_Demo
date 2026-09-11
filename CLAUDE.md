# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

Bare-metal firmware template for an STM32F429 (Cortex-M4) target, intended to integrate the
STM32F4xx HAL and Eclipse ThreadX (RTOS), built with CMake + Ninja and an `arm-none-eabi` GCC
toolchain. `core/`, `drivers/`, `bsp/`, and `app/` all have real sources now; only
`middlewares/threadx/` is still an empty stub (just a `CMakeLists.txt`, no ThreadX kernel/port
sources yet) — see Architecture below.

## Build

```bash
cmake -B build -G Ninja
cmake --build build
```

Post-build, the executable target (`${CMAKE_PROJECT_NAME}` = `stm32f429_firmware`) is converted to
`.hex`/`.bin` and its size is printed via `arm-none-eabi-objcopy`/`size`.

There is no test suite or linter configured in this repo yet.

Each module's `CMakeLists.txt` (`core`, `drivers`, `bsp`, `app`, `middlewares/threadx`) collects
its sources with a plain `file(GLOB ... "src/*.c")` — none use `CONFIGURE_DEPENDS`. This means
adding or removing a source file under an existing module directory is invisible to Ninja until
CMake is re-run: `cmake --build build` alone will silently keep building the old file list (no
error, the new file just never gets compiled). After adding/removing a `.c`/`.S` file, re-run
`cmake -B build -G Ninja` before `cmake --build build`.

### Editor / tooling

The root `CMakeLists.txt` sets `CMAKE_EXPORT_COMPILE_COMMANDS ON`, so every `cmake -B build`
(re)generates `build/compile_commands.json`. `.vscode/settings.json` points `clangd` at that file
and explicitly disables the Microsoft C/C++ extension's IntelliSense (`.vscode/extensions.json`
lists `ms-vscode.cpptools` as unwanted) — clangd is the intended source of code navigation/
diagnostics for this repo, not cpptools.

### Flashing

`jlink.cfg` at the repo root is a J-Link command file targeting `stm32f429ig` over SWD; it loads
`./build/stm32f429_firmware.bin` at `0x08000000`, verifies, resets, and runs:

```bash
JLinkExe -CommandFile jlink.cfg
```

Run this after a successful `cmake --build build` (the `.bin` path is relative to the repo root).

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
- `app/`'s and `middlewares/threadx`'s `CMakeLists.txt` guard their `file(GLOB ...)` with
  `if(NOT <SOURCES>)` and fall back to writing an empty stub `.c` into the build dir — modern CMake
  refuses `add_library(... OBJECT)`/`add_executable(...)` with zero sources. `app/` now has real
  sources (`app.c`) so its stub branch is dead/skipped; `middlewares/threadx` still has none, so it
  still falls back. The root executable target has the same problem for the same reason (all its
  code comes from the linked OBJECT libs) and gets a generated `exe_stub.c` the same way, plus an
  explicit
  `LINKER_LANGUAGE C` since a sourceless target can't infer one.

**Resolved (2026-09-10, superseded)**: `SysTick_Handler` (and every other real handler in
`core/src/stm32f4xx_it.c`) was silently never linked in, so `HAL_IncTick()` never ran, `uwTick`
never advanced, and any `HAL_Delay()` call hung forever — the LED blink loop would get stuck on
its first `HAL_Delay(500)`. Root cause: `startup_stm32f429xx.S` defines weak aliases
(`.thumb_set SysTick_Handler, Default_Handler`, an infinite self-loop) for every vector, in the
*same* object file as the vector table itself. Back when `core` was a STATIC library, the linker
only pulled an archive member when it had an outstanding undefined symbol — but the vector table's
reference to `SysTick_Handler` was already satisfied the moment `startup_stm32f429xx.o` got pulled
in (for `Reset_Handler`), so the linker never had a reason to also pull `stm32f4xx_it.o` out of the
same `core.a` for the strong override. Confirmed with `arm-none-eabi-nm`/`objdump`: `SysTick_Handler`
resolved to the exact same address as `Default_Handler`, disassembling to `b.n` (self-loop). The
same shadowing pattern separately hit `bsp` too (see `docx/TROUBLESHOOTING.md` problem 2:
`kservice.c`'s weak `rt_vsnprintf` was shadowing the full-featured strong one in `rt_vsnprintf.c`).
Both were first patched with `-Wl,--whole-archive ... -Wl,--no-whole-archive` around the affected
module(s) in the root `CMakeLists.txt`. That patch is gone now — see the OBJECT-library note right
below, which removes the whole bug class instead of patching each instance. Verify with
`arm-none-eabi-nm build/stm32f429_firmware.elf | grep SysTick_Handler` — its address must differ
from `Default_Handler`'s.

**Resolved**: all five module libraries (`app`, `bsp`, `threadx`, `drivers`, `core`) were switched
from `add_library(... STATIC)` to `add_library(... OBJECT)`. OBJECT libraries aren't archived into
a `.a`, so every one of their object files lands in the final link unconditionally — there's no
archive-extraction step left for a weak definition to "shadow" a strong one from a sibling source
file, which is what caused both bugs above. This also let the `-Wl,--whole-archive`/
`-Wl,--no-whole-archive` wrapping be removed from the root `CMakeLists.txt`'s
`target_link_libraries` entirely — it's meaningless for OBJECT libraries anyway (they were never
archives to begin with). Trade-off: none of these five targets can be distributed/reused as a
standalone `.a` outside this build anymore, but nothing in this repo needed that.

**Resolved**: `SystemClock_Config()` (`core/src/main.c`) targets **168 MHz**, not the F429's max
180 MHz — deliberate, not a mistake. `HSE_VALUE` (`core/inc/stm32f4xx_hal_conf.h`) was originally
left at ST's template default of 8 MHz while `PLLM=25` only makes sense for this board's actual
25 MHz HSE crystal (`PLLM=25` implies a 1 MHz PLL input, which requires HSE=25 MHz); the mismatch
threw every HAL clock calculation off by 180/57.6 ≈ 3.125×, garbling UART output and running
`HAL_Delay()` fast. Separately, CK48M (feeds USB OTG FS/SDIO/RNG) has no PLLSAI path on
STM32F429/439 (only F469/F479 have `RCC_CLK48CLKSOURCE_PLLSAIP`) — it's hardwired to the main
PLL's `Q` output only, and VCO=360MHz (the 180 MHz config) isn't a multiple of 48, so no integer
`PLLQ` gives an exact 48 MHz at 180 MHz SYSCLK. Retuned to `PLLN=336`/`PLLQ=7` (VCO=336MHz):
SYSCLK=168MHz, CK48M=48MHz exactly. Full writeups (with the `nm`/`objdump` verification steps) are
in `docx/TROUBLESHOOTING.md`, problem 3 and its two follow-ups.

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
`stm32f429_firmware.elf/.hex/.bin` (HAL + `bsp`'s LED/UART code + `app`'s init/task functions +
an empty `threadx` stub).

Toolchain note: `arm-none-eabi-gcc` on this dev machine is unpacked at a custom path
(`~/home/tools/arm-gnu-toolchain-*/bin`), not on `PATH` by default — add it to your shell profile
(`~/.zshrc`) rather than relying on ad hoc `export PATH=...` per session. `cmake`/`ninja` are
installed via Homebrew.

## Architecture

CMake module graph (each is an OBJECT library except the root executable and the
`stm32_mcu_flags` INTERFACE library — see the OBJECT-library "Resolved" entry under Build status
for why OBJECT rather than STATIC):

```
core        -> drivers, threadx, bsp, app, stm32_mcu_flags   (also owns startup_*.S and system_stm32f4xx.c)
drivers     -> stm32_mcu_flags
bsp         -> drivers, stm32_mcu_flags
threadx     -> stm32_mcu_flags
app         -> bsp, threadx, stm32_mcu_flags
```

`core/src/main.c` is now a thin entry point: `HAL_Init()` → `SystemClock_Config()` → `app_init()`,
then `while (1) app_task();`. All board-level/application logic (LED, UART console, the demo
`rt_kprintf` calls) lives in `app/src/app.c`, matching the intended layering — `core` depends on
`app` (not the other way around) purely so `main()` can call into it, the same way a vendor
"Templates" `main.c` calls into user application code.

The root `CMakeLists.txt` links the final `stm32f429_firmware` executable against
`app bsp threadx drivers core` — no special linker flags needed for any of them now that they're
OBJECT libraries; every one of their object files always lands in the final link. Toolchain flags
(`-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard`) and the linker script
(`cmake/STM32F429IGT6_FLASH.ld`) are centralized in the `stm32_mcu_flags` INTERFACE library and
propagated to every module through it, rather than being repeated per-target.

Layer responsibilities (intended, per the module layout):
- `drivers/` — vendored STM32F4xx HAL/LL drivers and CMSIS headers (`cminc/`); not meant to be
  hand-edited except for local fixes.
- `core/` — CMSIS device startup/system files and `main.c`/`stm32f4xx_it.c`/HAL MSP config —
  the vendor "Templates" style entry point and IRQ handlers. Also owns
  `stm32f4xx_hal_timebase_tim.c`, which overrides `HAL_InitTick()`/`HAL_SuspendTick()`/
  `HAL_ResumeTick()` to drive `uwTick` (and therefore `HAL_Delay()`) from TIM6 instead of SysTick —
  done in anticipation of ThreadX's own Cortex-M4 port claiming SysTick for the RTOS tick once
  `middlewares/threadx` gets real sources. `stm32f4xx_it.c`'s `SysTick_Handler()` is dead code
  right now (SysTick's interrupt is never enabled) and needs to be deleted once ThreadX supplies
  its own — leaving both would be a duplicate-symbol link error under the OBJECT-library setup.
- `bsp/` — board support layer, sits on top of `drivers`: `bsp_led.{c,h}` (GPIOB PB0/PB1 output-pin
  wrapper) and `bsp_uart.{c,h}` (USART1 PA9/PA10 init + RX IRQ), plus a ported subset of
  RT-Thread's `kservice.c`/`rt_vsnprintf.c` (`rtthread.h`) for `rt_kprintf()`-style formatted UART
  output — see `docx/TROUBLESHOOTING.md` for the porting bugs found along the way.
- `middlewares/threadx/` — intended to hold the ThreadX kernel (`common/`) and Cortex-M4 GNU port
  (`ports/cortex_m4/gnu/`) sources, referencing `tx_user.h` from `core/inc` (currently empty).
- `app/` — application/business logic, depends on `bsp` and `threadx`. `app.c` exposes `app_init()`
  (one-time setup: LED + UART init, demo `rt_kprintf` calls) and `app_task()` (the LED toggle +
  `HAL_Delay(500)` body of `core/src/main.c`'s main loop) — `app_task()` is written so it can later
  become a ThreadX thread entry function's loop body with minimal changes.

Toolchain file `cmake/arm-none-eabi.cmake` sets `CMAKE_SYSTEM_NAME Generic` and
`CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY` (required for cross-compiling bare-metal — the
toolchain can't link a runnable test executable), and gives compiled artifacts a `.elf` suffix.
