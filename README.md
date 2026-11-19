# FATAL FRAME Recompiled

A native Linux (and, later, Windows) static recompilation of *FATAL FRAME* (Xbox, NTSC-U, title ID `54430004`), made with [xboxrecomp](https://github.com/sp00nznet/xboxrecomp).

The game's original x86 code is lifted to C and compiled for the host. Nothing is emulated at the CPU level, although Xbox kernel services and hardware still require emulation. You need your own disc image; no game data is included in this repository.

Development is inspired by [NGBRecomp](https://github.com/gamboozino/ngbrecomp), with its techniques adapted to *FATAL FRAME*.

The compatible xboxrecomp toolkit currently lives at `../xboxrecomp`.

## Status

| Stage | State |
|---|---|
| Disc extraction, XBE parsing, disassembly and lift to C | Done, `tools/pipeline.sh` |
| Native Linux build (Clang, CMake, Ninja) | **Working** |
| First-launch installer and AppImage packaging | Implemented; AppImage boots |
| Xbox kernel initialization, memory mapping and guest entry point | **Working** |
| Direct3D initialization and GPU interrupts | **Working** |
| NV2A GPU (pushbuffer → Vulkan) | **Rendering** loading screen, logos and title screen |
| DirectSound/APU and audio output | **Working** for title-screen music |
| Main loop | **Running**; gameplay not yet reached |
| Controller input and saves | Not working or not verified |
| FMV playback | Not verified; title-screen attract sequence fades to black |
| Native Windows build | Not yet verified |

The game currently boots through the original Xbox startup sequence, displays the TECMO and KOEI logos, reaches the *FATAL FRAME* title screen and plays music.

The NV2A/Vulkan renderer is producing visible frames. However, controller input is not yet functional, and gameplay has not been reached.

## Build and run (Linux)

```sh
# Once: extract your disc and generate the C (about 2 minutes)
mkdir -p disc && cp "/path/to/Fatal Frame (USA).iso" "disc/Fatal Frame (USA).iso"      # or pass the path
tools/pipeline.sh "disc/Fatal Frame (USA).iso"

cmake -S . -B build/linux -G Ninja -DCMAKE_C_COMPILER=clang
ninja -C build/linux
build/linux/FatalFrame game_files
```

The game directory is the first argument, then `FFR_GAME_DIR`, then `game/` next to the executable.

Hard-disk images and saves default to `~/.local/share/FatalFrame`; `FFR_SAVE_DIR` overrides this location.

### AppImage

The project includes first-launch installation and AppImage packaging under `packaging/appimage/`.

An existing AppImage can be launched with:

```sh
build/appimage/FatalFrame-x86_64.AppImage game_files
```

The AppImage has been verified to reach the title screen with graphics and music.

## Bring-up tools

- `kill -USR1 <pid>` prints the guest thread's host backtrace and guest registers, helping locate stalls in recompiled functions.
- `RECOMP_WATCHDOG_SECS=N` enables the runtime watchdog.
- `tools/feedback.sh [secs]` collects indirect-call targets for additional function discovery and regeneration.
- `XBOX_LOG_LEVEL=4` and `RECOMP_KERNEL_LOG_BUDGET=100000` enable detailed kernel and filesystem logging.
- `FFR_WATCH_WRITE` and `FFR_DUMP_RAM` provide optional guest-memory diagnostics where supported.

Some defaults in `src/main.c` stand in for Xbox hardware behavior that is not yet fully implemented. These should eventually be replaced by accurate hardware emulation.

## Roadmap

1. **Controller input.** Connect host keyboard and gamepad input to the emulated Xbox controller interface so the title screen and menus become navigable.
2. **FMV playback.** Investigate the black screen during the title-screen attract sequence and implement any missing video playback functionality.
3. **Gameplay.** Progress beyond the title screen, identify additional missing functions and resolve runtime issues encountered during gameplay.
4. **Graphics and audio.** Continue improving NV2A/Vulkan rendering, synchronization, texture handling and APU audio behavior.
5. **Saves and stability.** Verify save data, filesystem behavior, guest threading and timing.
6. **Windows build.** Build and test the native Windows executable, `FatalFrame.exe`.
7. **Performance and packaging.** Improve rendering performance, frame pacing, AppImage distribution and first-launch installation.

## Disclaimer

*FATAL FRAME Recompiled* is an independent, unofficial project. It does not distribute copyrighted game assets and is not affiliated with or endorsed by Koei Tecmo or Microsoft.