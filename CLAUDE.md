# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

Bare-metal firmware template for an STM32F429 (Cortex-M4) target, integrating the STM32F4xx HAL
and Eclipse ThreadX (RTOS), built with CMake + Ninja and an `arm-none-eabi` GCC toolchain.
`core/`, `drivers/`, `bsp/`, and `app/` all have real sources; `middlewares/threadx/` now also has
real sources (the vendored ThreadX kernel + Cortex-M4 GNU port), and `app/src/app.c` runs real
ThreadX threads via `tx_kernel_enter()`/`tx_application_define()`. The build links successfully as
of 2026-09-17 — see the "Resolved (2026-09-17)" entry under Build status for four ThreadX-porting
link errors (a source-glob miss, an example-port symbol-name mismatch, and two duplicate-handler
conflicts with `core/src/stm32f4xx_it.c`) that were hit and fixed getting there; read it before
touching `middlewares/threadx/` or `core/src/stm32f4xx_it.c` again.

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

### Compiler-standard pin on `bsp/src/usmart.c`

`bsp/CMakeLists.txt` force-compiles just that one file with `-std=gnu17` (via
`set_source_files_properties(... COMPILE_OPTIONS "-std=gnu17")`), overriding the project-wide
default. `usmart.c` (a ported ALIENTEK "usmart" runtime function-invocation console — see
Architecture below) calls arbitrary registered functions through an unprototyped `u32(*)()`
function-pointer type, a classic K&R-style trick that is legal when an empty parameter list means
"unspecified arguments" (true through C17/gnu17). GCC 15's default `-std=gnu23` instead treats
`()` as `(void)`, turning every one of those calls into a hard "too many arguments" error. If you
add a new file that leans on the same trick, either add a similar per-file override or give the
function pointer an explicit variadic/typed prototype instead.

### Build status

**Resolved (2026-09-17)**: the ThreadX integration landed in commit `9c8814b`
("bug(): 没有编译通过") — `middlewares/threadx/` gained real ThreadX kernel (`common/`) and
Cortex-M4 GNU port (`ports/`) sources, and `app/src/app.c` was rewritten to start ThreadX
(`tx_kernel_enter()` at the end of `app_init()`, tasks created in `tx_application_define()`) — but
`cmake -B build -G Ninja && cmake --build build` failed at the final link step with four
independent root causes, found and fixed together:

1. **`middlewares/threadx/CMakeLists.txt`'s glob missed most port `.S` files.** It globbed
   `"ports/src/*.c" "ports/*.S"`, but every ThreadX Cortex-M4 port assembly file actually lives
   under `ports/src/*.S` (`tx_thread_schedule.S`, `tx_thread_stack_build.S`,
   `tx_thread_context_save.S`/`_restore.S`, `tx_thread_system_return.S`, `tx_timer_interrupt.S`,
   `tx_thread_interrupt_control.S`/`_disable.S`/`_restore.S`, `tx_misra.S`) — only
   `ports/tx_initialize_low_level.S` (which sits directly under `ports/`, not `ports/src/`) matched.
   Confirmed via the link error: undefined references to `_tx_thread_schedule`,
   `_tx_thread_stack_build`, and `_tx_timer_interrupt`. **Fix**: added `"ports/src/*.S"` as a third
   glob pattern.
2. **`tx_initialize_low_level.S` references symbol names this project's linker script/startup file
   don't define.** This file is ThreadX's official `ports/cortex_m4/gnu/example_build` template —
   copying it verbatim is correct, but it's written to pair with *ThreadX's own* example linker
   script, which defines `_vectors` (vector table) and `__RAM_segment_used_end__` (first free RAM
   address for `tx_application_define`). This project instead uses an ST/STM32CubeMX-generated
   startup file and linker script, where those same concepts are named `g_pfnVectors`
   (`core/src/startup_stm32f429xx.S`) and `_end`/`end` (`cmake/STM32F429IGT6_FLASH.ld`) — confirmed
   via undefined references to both ThreadX-side names. This symbol-name mismatch is expected
   whenever ThreadX's example port file is dropped into a non-ThreadX-demo project skeleton, not a
   copy mistake. **Fix**: rather than hand-editing the vendored ThreadX file, added two alias
   assignments to `cmake/STM32F429IGT6_FLASH.ld` right after `._user_heap_stack`:
   `__RAM_segment_used_end__ = _end;` and `_vectors = g_pfnVectors;`.
3. **`SysTick_Handler` was a duplicate-definition link error, exactly as flagged in the Architecture
   note below.** `core/src/stm32f4xx_it.c` still strong-defined `SysTick_Handler()` (already noted
   as dead code needing deletion once ThreadX supplied its own), and
   `tx_initialize_low_level.S` supplies ThreadX's own `SysTick_Handler`. Under the OBJECT-library
   setup both object files land in the final link unconditionally, so this was a hard
   `multiple definition of 'SysTick_Handler'` error, not a silent weak-shadowing bug. **Fix**:
   deleted the `SysTick_Handler()` body from `core/src/stm32f4xx_it.c` and its prototype from
   `core/inc/stm32f4xx_it.h`.
4. **`PendSV_Handler` was the same duplicate-definition problem, one level deeper.** Fixing #1
   above pulled in `middlewares/threadx/ports/src/tx_thread_schedule.S`, which defines
   `PendSV_Handler` for ThreadX's Cortex-M4 thread context-switch mechanism — colliding with the
   empty `void PendSV_Handler(void) {}` stub in `core/src/stm32f4xx_it.c`. This one only surfaces
   *after* #1 is fixed (the port source has to actually be linked in to collide), so it wasn't
   visible in the original error output. **Fix**: same pattern as #3 — deleted the body from
   `stm32f4xx_it.c` and the prototype from `stm32f4xx_it.h`. `SVC_Handler` and `DebugMon_Handler`
   were checked and are *not* redefined anywhere in the ThreadX port, so they were left alone.

Verified: `cmake -B build -G Ninja && cmake --build build` now exits 0 and produces
`stm32f429_firmware.elf/.hex/.bin`. `arm-none-eabi-nm build/stm32f429_firmware.elf | grep -E
"SysTick_Handler|PendSV_Handler"` shows both resolving to addresses inside the ThreadX port's code
range, distinct from `Default_Handler`.

As of 2026-09-10, verified against a real `arm-none-eabi-gcc` 15.3.1 + CMake 4.4.3 + Ninja 1.13.2
toolchain (all three items below were true *before* the ThreadX integration above; they are not
retested by the current build failure since it never reaches a successful link):

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
  refuses `add_library(... OBJECT)`/`add_executable(...)` with zero sources. `app/`'s and
  `middlewares/threadx`'s stub branches were both dead/skipped as of 2026-09-10 (`app/` had
  `app.c`; `middlewares/threadx` was still empty at that point and fell back to the stub). As of
  2026-09-17 `middlewares/threadx` also has real sources, so both stub branches are now dead — the
  `if(NOT <SOURCES>)` guards are effectively vestigial unless a module is emptied out again. The
  root executable target has the same problem for the same reason (all its code comes from the
  linked OBJECT libs) and gets a generated `exe_stub.c` the same way, plus an explicit
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
restore the real `Legacy/stm32_hal_legacy.h` or define `PHY_READ_TO`/`PHY_WRITE_TO` yourself. As of
2026-09-10, `cmake -B build -G Ninja && cmake --build build` succeeded end-to-end and produced
`stm32f429_firmware.elf/.hex/.bin` (HAL + `bsp`'s LED/UART code + `app`'s init/task functions + an
empty `threadx` stub) — real ThreadX sources landed afterward and broke that build for a while (see
the "Resolved (2026-09-17)" entry above), but the build links successfully again as of 2026-09-17.

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
  done in anticipation of ThreadX's own Cortex-M4 port claiming SysTick for the RTOS tick now that
  `middlewares/threadx` has real sources. `stm32f4xx_it.c` no longer defines `SysTick_Handler()` or
  `PendSV_Handler()` — both are now supplied by ThreadX's port
  (`middlewares/threadx/ports/tx_initialize_low_level.S` and `.../ports/src/tx_thread_schedule.S`
  respectively); leaving the old stubs in place was a `multiple definition` link error under the
  OBJECT-library setup, fixed in the "Resolved (2026-09-17)" entry under Build status.
- `bsp/` — board support layer, sits on top of `drivers`:
  - `bsp_led.{c,h}` (GPIOB PB0/PB1 output-pin wrapper).
  - `bsp_uart.{c,h}` (USART1 PA9/PA10 init + hand-written `USART1_IRQHandler` RX-into-line-buffer
    logic, not `HAL_UART_Receive_IT`), plus a ported subset of RT-Thread's
    `kservice.c`/`rt_vsnprintf.c` (`rtthread.h`) for `rt_kprintf()`-style formatted UART output.
  - `bsp_key.{c,h}` — ALIENTEK-style debounced key/button driver (`KID_KEY0..2`/`KID_KEYUP`,
    down/up/long-press events into a FIFO). Scanned every 10 ms from its *own* dedicated **TIM7**
    interrupt (`bsp_KeyTimerInit()`), deliberately not sharing `core`'s TIM6 HAL timebase: TIM6
    starts ticking from `HAL_Init()`, well before `app_init()`'s `bsp_InitKey()` has set up the key
    state/GPIOs, so reusing it would scan uninitialized state. `TIM7_IRQHandler` also does its own
    flag read/clear instead of going through the shared weak `HAL_TIM_PeriodElapsedCallback` —
    `core`'s TIM6 timebase already provides a strong definition of that callback, and under the
    OBJECT-library setup a second strong definition would be a duplicate-symbol link error (the
    same class of pitfall as the weak-shadowing bugs below, just avoided this time instead of hit).
  - `usmart.{c,h}`/`usmart_str.{c,h}` — a ported ALIENTEK "usmart" runtime console: register a C
    function's pointer + a string prototype in a table (see `app/src/usmart_config.c`) and invoke
    it by name/args over UART. `usmart_dev.init()` is called from `app_init()`, and
    `USART1_IRQHandler` fills the line buffer it reads from. Contrary to an earlier note here, the
    console *is* actively pumped: `usmart_init()` (`bsp/src/usmart.c`) itself calls
    `Timer4_Init()` when `USMART_ENTIMX_SCAN==1` (the default, `bsp/inc/usmart.h`), which enables a
    TIM4 interrupt (`TIM4_IRQHandler`) that calls `usmart_dev.scan()` every 100ms independent of
    `app_task()`/ThreadX — so `list`/`led_set(...)`-style commands typed over UART work today, not
    just RX buffering. See the compiler-standard note under Build above for its build-flag quirk.
  - See `docx/TROUBLESHOOTING.md` for the UART/`rt_kprintf` porting bugs found along the way.
- `middlewares/threadx/` — the vendored ThreadX kernel (`common/{inc,src}`) and Cortex-M4 GNU port
  (`ports/inc`, `ports/src/*.S`, plus `ports/tx_initialize_low_level.S`), with
  `tx_user.h`/`tx_port.h` under `ports/inc` and a project `tx_user.h` referenced from `core/inc`.
  `target_compile_definitions(threadx PUBLIC TX_INCLUDE_USER_DEFINE_FILE)` in
  `middlewares/threadx/CMakeLists.txt` pulls that user config in. `tx_initialize_low_level.S` is
  ThreadX's stock `ports/cortex_m4/gnu/example_build` file — copied verbatim, its `_vectors`/
  `__RAM_segment_used_end__` references are resolved onto this project's `g_pfnVectors`/`_end` via
  aliases added to `cmake/STM32F429IGT6_FLASH.ld`, rather than editing the vendored file. See the
  "Resolved (2026-09-17)" entry under Build status for this and the other three build/link bugs hit
  wiring this module in.
- `app/` — application/business logic, depends on `bsp` and `threadx`. `app.c` calls real ThreadX
  APIs (`#include "tx_api.h"`): `app_init()` does one-time board setup (LED + UART init,
  `bsp_InitKey()`/`bsp_KeyTimerInit()`, `usmart_dev.init()`, demo `rt_kprintf` calls) and then never
  returns — it ends by calling `tx_kernel_enter()`, so `core/src/main.c`'s `while (1) app_task();`
  is only reached if ThreadX fails to start. ThreadX itself calls back into `app.c`'s
  `tx_application_define()` to create the initial threads: `AppTaskStart` (priority 2, which in
  turn creates the rest of the threads/mutex via `AppTaskCreate()`/`AppObjCreate()` and then sleeps
  forever), `AppTaskStat`/`AppTaskIDLE` (CPU-usage statistics, priorities 30/31). `AppTaskCreate()`
  adds `AppTaskLed` and `AppTaskKey` (both priority 3) — `AppTaskLed` does the old LED-toggle loop
  via `tx_thread_sleep(500)` instead of `HAL_Delay`, `AppTaskKey` polls `bsp_GetKey()` in a
  `tx_thread_sleep(5)` loop and prints key events through `App_Printf()` (a `tx_mutex`-guarded
  wrapper around `rt_kprintf`, currently `#if 0`'d out/dead — see the body of `App_Printf()`).
  `app_task()`/`app_led_test()`/`app_key_test()` (the pre-ThreadX bare-metal loop body, `app.h`)
  are now dead code reachable only in the ThreadX-fails-to-start fallback path. `usmart_config.c` is
  where callable functions (e.g. `led_set()`, `app.h`'s `led_set` wrapper around `bsp_led`) get
  registered into usmart's name table.

Toolchain file `cmake/arm-none-eabi.cmake` sets `CMAKE_SYSTEM_NAME Generic` and
`CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY` (required for cross-compiling bare-metal — the
toolchain can't link a runnable test executable), and gives compiled artifacts a `.elf` suffix.
