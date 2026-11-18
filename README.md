# FATAL FRAME Recompiled

A native Linux (and, later, Windows) static recompilation of *FATAL FRAME* (Xbox, NTSC-U, title ID `54430004`), made with [xboxrecomp](https://github.com/sp00nznet/xboxrecomp).

The game's original x86 code is lifted to C and compiled for the host. Xbox CPU instructions are not interpreted at runtime, although Xbox kernel services and hardware still require emulation.

You need your own copy of the game. No copyrighted game files are included in this repository.

Development is inspired by [NGBRecomp](https://github.com/gamboozino/ngbrecomp), with its techniques and milestones adapted to *FATAL FRAME*.

The current development environment is Linux (Ubuntu/WSL), with native Windows support planned.

## Status

| Stage | State |
|---|---|
| Disc extraction, XBE parsing, disassembly and lift to C | Implemented; generated sources build and execute |
| Native Linux build (Clang, CMake, Ninja) | **Working** |
| Xbox memory mapping, kernel initialization and guest entry point | **Working** |
| Xbox filesystem and game resource loading | Working for tested startup resources |
| Direct3D initialization | Reaches NV2A initialization and registers the GPU interrupt handler |
| NV2A renderer (Vulkan) | Initializes and opens a window; output remains black |
| DirectSound/APU | Emulated APU initializes; in-game audio not yet verified |
| Main loop | Not confirmed; boot thread observed waiting on a D3D event |
| Controller input and saves | Not verified |
| XAPI fibers | Not implemented; usage in *FATAL FRAME* not established |
| Native Windows build | Not yet verified |

The recompiled executable successfully starts the original Xbox game code, initializes the emulated Xbox runtime, and loads multiple resources from the extracted disc, including fonts, message data and textures.

The current build opens a Vulkan window but does not yet display the game's title screen or enter confirmed gameplay. The guest boot thread has been observed waiting inside the recompiled Direct3D routine `sub_000E6ED0`.

The game registers a GPU interrupt handler at `0x000EFD50`. Investigation of interrupt dispatch and VBlank signaling is ongoing.

## Build and run (Linux)

The project currently builds using Clang, CMake and Ninja on Linux.

The xboxrecomp toolkit is expected at `../xboxrecomp` relative to the project directory.

```sh
# Once: extract your disc and generate the C (about 2 minutes)
mkdir -p disc && cp "/path/to/Fatal Frame (USA).iso" "disc/Fatal Frame (USA).iso"      # or pass the path
tools/pipeline.sh "disc/Fatal Frame (USA).iso"

cmake -S . -B build/linux -G Ninja -DCMAKE_C_COMPILER=clang
ninja -C build/linux
build/linux/FatalFrame game_files
```

The executable accepts the game directory as its first argument, then checks `FFR_GAME_DIR`, followed by a `game/` directory beside the executable.

The extracted game directory must contain `default.xbe` and the game's original assets.

Save data defaults to `~/.local/share/FatalFrame` on Linux. Set `FFR_SAVE_DIR` to override this location.

### Windows

The intended Windows executable is `FatalFrame.exe`.

A native Windows build and installation workflow are planned but have not yet been verified. The current Linux implementation should not be considered a finished Windows release.

## Bring-up tools

- `kill -USR1 <pid>` samples the guest boot thread, printing its host backtrace and guest registers. This is useful for identifying stalls in lifted functions.
- `RECOMP_WATCHDOG_SECS=N` enables the runtime watchdog for detecting stalled execution.
- `tools/feedback.sh [secs]` supports collecting indirect-call targets and feeding them back into the recompilation pipeline. Availability depends on the build and instrumentation.
- `XBOX_LOG_LEVEL=4` and `RECOMP_KERNEL_LOG_BUDGET=100000` enable additional kernel and filesystem diagnostics where supported.
- `RECOMP_VBLANK=1` enables the experimental VBlank interrupt timer.
- `FFR_RENDERER=none` selects the no-GPU bring-up path for investigating guest execution independently of Vulkan rendering.

Some runtime settings compensate for Xbox hardware behavior that is not yet fully implemented. These are development aids rather than permanent solutions.

The DSP mailbox acknowledgment override (`RECOMP_DSP_ACK`) remains disabled until *FATAL FRAME*'s actual requirements and mailbox address are verified.

The experimental `src/fibers.c` is not part of the active implementation. It contains NGBRecomp-derived code that has not been adapted or validated for *FATAL FRAME*.

## Current development priorities

1. **GPU interrupts and synchronization.** Investigate the Direct3D interrupt handler at `0x000EFD50`, its generated function entry and VBlank delivery. Determine why the boot thread remains blocked in a Direct3D event wait.

2. **Main-loop execution.** Advance through the remaining startup routines, identify missing indirect-call targets and verify that the game's main loop runs and submits GPU commands.

3. **Graphics.** Continue developing the NV2A/Vulkan renderer, including pushbuffer execution, texture handling, synchronization and correct frame presentation.

4. **Audio and input.** Validate DirectSound initialization, APU playback and controller input once the game reaches the appropriate initialization paths.

5. **Gameplay and stability.** Resolve remaining kernel, filesystem, threading, memory and timing issues until the title screen, menus and gameplay function reliably.

6. **Native Windows support.** Build and test `FatalFrame.exe`, sharing as much of the runtime as possible between Linux and Windows.

7. **Installation and performance.** Implement first-launch disc validation and extraction, improve compatibility and optimize the recompiled executable.

## Project goals

The long-term goal is a faithful, stable and playable native PC version of the original Xbox release of *FATAL FRAME*.

The planned distribution will use a portable executable that installs the necessary game files from the user's own original disc image.

The project does not distribute game assets and is not affiliated with or endorsed by Koei Tecmo.

Additional features such as cheats, alternative controller prompts and expanded configuration options are not current development priorities.