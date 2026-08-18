# 01 — Toolchain, CMake, and Visual Studio Code

STATUS: IN_PROGRESS

## Goal

Create the canonical firmware project foundation before implementing hardware features.

At completion, a fresh clone should support reproducible host tests and STM32 cross-compilation through CMake, and the repository should open cleanly in VS Code without requiring STM32CubeIDE project metadata.

## Prerequisites

- `AGENTS.md` reviewed;
- current pinout and MCU confirmed;
- GNU Arm Embedded compiler installation strategy documented;
- no existing build system that must be preserved.

## In scope

- C17 project skeleton;
- CMake layout;
- Arm GCC toolchain file;
- build presets;
- host-test harness;
- warning policy;
- generated compile database;
- VS Code workspace/configuration;
- minimal embedded target proving startup/linking strategy;
- minimal host library/test proving shared pure-code compilation;
- build documentation;
- artifact naming and version hooks.

## Out of scope

- final clock configuration;
- UART driver implementation beyond any minimal link stub required to prove the target;
- TFT/W25Q;
- relay/range control;
- ADC/DMA;
- measurement DSP;
- product UI.

## Expected repository additions

Recommended structure:

```text
Firmware/
├── CMakeLists.txt
├── cmake/
│   ├── toolchains/
│   │   └── arm-none-eabi-gcc.cmake
│   ├── modules/
│   │   ├── CompilerWarnings.cmake
│   │   └── BuildOptions.cmake
│   └── stm32/                  # only if required by chosen CMSIS/HAL integration
├── src/
│   └── ...
├── tests/
│   ├── CMakeLists.txt
│   └── smoke/
└── third_party/

CMakePresets.json               # location decided and documented
```

Do not create unnecessary abstraction layers merely because this is a new project.

## Task 1 — Decide CMake root/preset placement

Evaluate two clean options:

### Option A

Root repository owns `CMakePresets.json`, with `Firmware/` as source directory.

### Option B

`Firmware/` owns both `CMakeLists.txt` and `CMakePresets.json`.

Choose one and document the command-line workflow. Prefer the option that keeps firmware commands simple and VS Code CMake Tools predictable.

Acceptance:

- no absolute local paths;
- presets are usable from documented working directory;
- host and STM32 configurations are clearly distinct.

## Task 2 — Create Arm GCC toolchain file

The toolchain file should define:

- `CMAKE_SYSTEM_NAME Generic`;
- processor/target identity;
- `arm-none-eabi-gcc`;
- `arm-none-eabi-objcopy`;
- `arm-none-eabi-size`;
- appropriate try-compile behavior for bare metal;
- CPU flags for Cortex-M3;
- Thumb mode;
- section garbage collection strategy;
- debug/release optimization policy.

Expected target architecture flags include the Cortex-M3/Thumb equivalents, but the agent must verify exact GNU syntax.

Do not add FPU flags: STM32F103 Cortex-M3 has no hardware FPU.

## Task 3 — Establish C17 policy

Configure project-owned targets for C17 without compiler extensions unless a documented low-level file requires them.

Desired semantic baseline:

```text
C_STANDARD 17
C_STANDARD_REQUIRED ON
C_EXTENSIONS OFF
```

Vendor code can be isolated from stricter project-owned policy as needed.

## Task 4 — Warning policy

Create a centralized warning module for project-owned code.

Start with a curated GCC warning set such as:

```text
-Wall
-Wextra
-Wpedantic
-Wshadow
-Wconversion
-Wsign-conversion
-Wdouble-promotion
-Wformat=2
-Wundef
```

Evaluate warnings against CMSIS/HAL separately. Do not globally suppress useful warnings because vendor headers are noisy.

Decide/document whether `-Werror` is enabled:

- host tests may use it if practical;
- embedded project-owned code may use it after vendor-header boundaries are clean;
- third-party/vendor targets should not force the project to disable warnings globally.

## Task 5 — Dependency strategy for CMSIS / STM32CubeF1

Choose a reproducible strategy, for example:

- vendored/submodule version pinned to a specific upstream revision;
- CMake FetchContent only if deterministic/offline implications are acceptable;
- checked-in minimum required CMSIS/device/HAL files.

Requirements:

- exact version/commit is identifiable;
- license notices are preserved;
- a new developer can reproduce the build;
- no dependency on files generated only inside a personal STM32CubeIDE workspace.

Record the decision in `docs/10-Consolidated-Design-Decisions.md` if it is consequential.

## Task 6 — Linker/startup strategy

Provide or integrate:

- STM32F103C8T6 linker script;
- startup/vector-table source;
- system initialization entry points required by CMSIS;
- correct Flash/RAM memory regions;
- `.data` / `.bss` initialization;
- stack/heap policy.

The embedded target should link without requiring a heap. If `_sbrk`/newlib stubs are needed, implement them conservatively and ensure runtime-critical firmware does not depend on dynamic allocation.

The agent must verify the exact Flash/RAM assumptions against the target module baseline and avoid silently assuming clone-specific oversized Flash.

## Task 7 — Minimal embedded target

Create the smallest useful firmware target that proves:

- startup works at link level;
- C runtime initialization resolves;
- `main()` is compiled;
- linker script is used;
- `.elf` is produced;
- `.bin` and `.hex` are generated after build;
- size information is printed or available;
- map file can be generated.

Do not implement final peripherals in this phase.

The minimal `main()` should preserve safe intent and avoid driving unknown outputs until Phase 02 establishes the real GPIO initialization.

## Task 8 — Host-test harness

Choose a small C-compatible test approach. Options may include:

- a lightweight vendored C test framework;
- CTest with small custom test executables/assert helpers;
- another dependency with a clear license and low complexity.

Requirements:

- host tests run without Arm compiler;
- `ctest` is the canonical runner;
- pure modules can later be linked into host tests;
- at least one smoke test demonstrates the path.

Do not introduce C++ only to obtain a test framework.

## Task 9 — CMake presets

Create at least conceptual equivalents of:

```text
host-debug
host-release
stm32-debug
stm32-release
```

Build presets should map cleanly to configure presets.

Test presets should cover host tests.

Consider enabling `CMAKE_EXPORT_COMPILE_COMMANDS` for development configurations.

## Task 10 — VS Code integration

Ensure `WTK.RLCMeter.code-workspace` and `.vscode/` settings work with the final CMake layout.

Recommended extensions:

- CMake Tools;
- Microsoft C/C++ or another agreed C language engine;
- Cortex-Debug for future SWD work.

Requirements:

- no machine-specific compiler path if it can be discovered through PATH/toolchain config;
- IntelliSense should consume CMake configuration / compile commands;
- opening the workspace must not rewrite the canonical build configuration;
- configure-on-open may be enabled only if the expected toolchain behavior is reasonable; otherwise document manual configuration.

## Task 11 — Formatting/editor policy

Add or validate `.editorconfig` and C source conventions.

Do not introduce an aggressive formatter configuration before existing style conventions are settled. A later phase may add `clang-format`, but the initial build should not be blocked by formatting tooling.

## Task 12 — Build metadata

Create a strategy for firmware version metadata that can later expose:

- semantic/project version;
- Git commit/hash where available;
- build type;
- hardware compatibility constant;
- calibration schema version.

Do not make Git mandatory at runtime/configure time if source archives need to build; provide fallbacks.

## Task 13 — Documentation

Update `Firmware/README.md` with exact commands that were actually tested.

Example end-state style:

```bash
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug

cmake --preset stm32-debug
cmake --build --preset stm32-debug
```

Document prerequisites for Windows/Linux development where practical, especially the Arm GCC requirement.

## Automated acceptance criteria

The phase is complete automatically when:

- host configure succeeds;
- host build succeeds;
- host smoke tests pass through CTest;
- STM32 configure succeeds with `arm-none-eabi-gcc` available;
- embedded target links;
- `.elf`, `.bin`, and `.hex` are produced;
- size/map output is available;
- no C++ source or Arduino dependency is introduced;
- workspace/config files contain no developer-specific absolute paths.

## Manual acceptance criteria

- open `WTK.RLCMeter.code-workspace` in VS Code;
- CMake Tools recognizes the firmware project;
- source navigation/IntelliSense works after configuration;
- documented commands match actual behavior.

No target board is required to complete this phase.

## Expected handoff

Report:

- chosen dependency strategy;
- chosen test framework;
- chosen preset structure;
- exact build commands and outputs;
- warning policy;
- generated artifact paths;
- any assumptions about STM32F103C8T6 Flash/RAM;
- readiness for Phase 02.

## Explicit stop conditions

Stop and report rather than inventing a workaround if:

- current hardware target identity is ambiguous;
- available CMSIS/device files disagree with STM32F103C8T6;
- linker memory map cannot be verified;
- a proposed dependency has incompatible licensing;
- the only proposed solution requires switching to Arduino/PlatformIO/C++ against the architecture baseline.
