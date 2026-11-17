# FATAL FRAME Recompiled

A native Windows (and, later, Linux) static recompilation of *FATAL FRAME* (Xbox, NTSC-U, title ID `54430004`), made with [xboxrecomp](https://github.com/sp00nznet/xboxrecomp). The game's x86 code is lifted to C and compiled for the host. Nothing is emulated at the CPU instruction level, although Xbox hardware and kernel services still require emulation. You need your own disc image; no game data is included in this repository.

Development follows relevant milestones from [NGBRecomp](https://github.com/gamboozino/ngbrecomp), adapted for the original Xbox version of *FATAL FRAME*.

## Status

| Stage | State |
|---|---|
| Disc extraction and XBE parsing | done |
| Disassembly, function identification, ABI analysis and lift to C | pipeline prepared; not yet verified end-to-end |
| Native Windows build (MSVC, x86-64) | not yet verified |
| Xbox kernel initialization, memory mapping and guest entry point | implemented in host source; not yet verified |
| Direct3D initialization and rendering | not started |
| DirectSound and audio output | not started |
| Main loop | not reached |
| Controller input and saves | not verified |
| Linux build | planned |

An earlier experimental implementation successfully built on Windows, initialized the Xbox runtime and reached the game's entry point. Its findings and fixes will be brought into this repository as needed.

## Build and run (Windows)

The Windows build is still being brought up. The current Bash pipeline generates C sources using xboxrecomp and requires a compatible Bash environment with Python 3 and Capstone.

```sh
# Extract the disc if needed and generate the recompiled C sources
bash tools/pipeline.sh "disc/Fatal Frame (USA).iso"
```

The project is intended to build as a native Windows executable named `FatalFrame.exe` using CMake and MSVC. A complete build procedure will be documented once verified.

The game directory is the first argument, then `FFR_GAME_DIR`, then `game/` next to the executable. Saves currently default to `%LOCALAPPDATA%\FatalFrame` on Windows (`FFR_SAVE_DIR` overrides).

## Bring-up tools

- `RECOMP_WATCHDOG_SECS=N` enables the runtime watchdog for detecting stalled guest execution.
- `tools/feedback.sh [secs]` is intended to collect indirect-branch targets, feed them back into function detection and regenerate the code. It requires an instrumented build and is not yet verified for Windows.
- `XBOX_LOG_LEVEL=4` and `RECOMP_KERNEL_LOG_BUDGET=100000` enable detailed kernel and file-access diagnostics where supported.
- On Linux, `kill -USR1 <pid>` is intended to print the guest thread's host backtrace and registers. This functionality has not yet been tested.

NGBRecomp-specific pushbuffer fence and DSP mailbox workarounds have been left disabled until Fatal Frame's actual hardware behavior can be established.

## Roadmap

1. **Native Windows bring-up.** Complete the recompilation pipeline, compile `FatalFrame.exe`, initialize the Xbox kernel and reach the game's main loop.
2. **Graphics.** Get Fatal Frame's Xbox D3D8 code rendering correctly. Start with xboxrecomp's existing D3D11 backend while evaluating NGBRecomp's NV2A/Vulkan approach if needed.
3. **Audio and input.** Bring up DirectSound/APU audio output and Xbox controller support.
4. **Gameplay stability.** Fix guest execution, kernel compatibility, filesystem, saving and synchronization issues until the original game runs reliably.
5. **Installation and performance.** Provide a portable executable that installs from the user's original disc image, improve runtime performance and eliminate unnecessary compatibility workarounds.
6. **Linux support.** Investigate native Linux builds after the Windows implementation is functional.

The priority is a faithful, stable, playable version of the original game. Cheats, alternative controller prompts, recomp-specific settings and additional content are not currently planned.
