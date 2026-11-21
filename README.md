# FATAL FRAME Recompiled

A native Linux (and, later, Windows) static recompilation of *FATAL FRAME* (Xbox, NTSC-U, title ID `54430004`), made with [xboxrecomp](https://github.com/sp00nznet/xboxrecomp).

The game's original x86 code is lifted to C and compiled for the host. Nothing is emulated at the CPU level, although Xbox kernel services and hardware still require emulation. You need your own disc image; no game data is included in this repository.

Development is inspired by [NGBRecomp](https://github.com/gamboozino/ngbrecomp), with its techniques adapted to *FATAL FRAME*.

The compatible xboxrecomp toolkit currently lives at `../xboxrecomp` and includes Linux DVD filesystem and kernel handle-management fixes needed by this project.

## Status

| Stage | State |
|---|---|
| Disc extraction, XBE parsing, disassembly and lift to C | Done, `tools/pipeline.sh` |
| Native Linux build (Clang, CMake, Ninja) | **Working** |
| First-launch installer and AppImage packaging | Implemented; AppImage boots |
| Pre-launch settings screen | **Working**; settings saved between launches |
| Xbox kernel initialization, memory mapping and guest entry point | **Working** |
| Direct3D initialization, GPU interrupts and synchronization | **Working** |
| NV2A GPU (pushbuffer → Vulkan) | **Rendering** startup sequences, title screen and FMV video |
| DirectSound/APU initialization | **Working**; DSP effects commands acknowledged, not executed |
| Audio output | **Working** for title music and FMVs; intermittent FMV audio glitches |
| DVD filesystem and file handles | Case-insensitive disc lookup and duplicate-handle management implemented |
| Main loop | **Running**; gameplay not yet reached |
| FMV playback | **Working**; title-screen attract movies play, finish and return to title |
| Controller input | Not yet functional in-game |
| Saves and gameplay | Not yet verified |
| Native Windows build | Not yet verified |

The game boots through its original Xbox startup sequence, displays the TECMO startup logo, reaches the *FATAL FRAME* title screen and plays music. The **Skip intro logo** setting can bypass the TECMO logo without affecting any FMV cinematics.

The NV2A/Vulkan renderer produces visible graphics and plays the title-screen attract movies (`scene9100.wmv` and `scene9200.wmv`). Both movies have been observed reaching natural completion, releasing their playback resources and returning to the title screen with graphics and music restored.

FMV audio decoding is functional, although intermittent silence and audio glitches remain in the DirectSound/APU playback path. The DSP effects image loads successfully, but its commands are acknowledged through a compatibility workaround rather than executed by an emulated DSP.

In-game controller input is not yet functional, so New Game navigation, gameplay and saving remain unverified.

## Build and run (Linux)

```sh
# Once: extract your disc and generate the C (about 2 minutes)
mkdir -p disc && cp "/path/to/Fatal Frame (USA).iso" "disc/Fatal Frame (USA).iso"      # or pass the path
tools/pipeline.sh "disc/Fatal Frame (USA).iso"

cmake -S . -B build/linux -G Ninja -DCMAKE_C_COMPILER=clang
ninja -C build/linux
build/linux/FatalFrame game_files
```

The game directory is selected from the first argument, then `FFR_GAME_DIR`, then `game/` next to the executable.

Hard-disk images, saves and cache files default to `~/.local/share/FatalFrame`; `FFR_SAVE_DIR` overrides this location.

### Settings

A settings screen appears before the game starts, supporting keyboard, mouse and controller navigation within the launcher.

Available settings include:

- Window size
- Windowed or fullscreen display
- Render resolution (1×–4×)
- V-Sync
- **Skip intro logo** — skips only the TECMO startup logo
- Show settings screen at startup

The game preserves its original **4:3 aspect ratio**.

Settings are saved to `~/.config/FatalFrame/settings.ini` (or under `XDG_CONFIG_HOME`). Run with `--settings` to reopen the screen if it has been disabled.

F11 or Alt+Enter toggles fullscreen during execution.

Environment variables override saved settings for testing:

- `FFR_FULLSCREEN=0|1`
- `FFR_VSYNC=0|1`
- `NV2A_SCALE=n`
- `FFR_NO_SETTINGS=1` — bypass the settings screen

The intro-logo setting does not automatically skip New Game, story or title-screen attract cinematics. Those retain the original game's playback behavior.

### AppImage

The project includes first-launch installation and AppImage packaging under `packaging/appimage/`.

An existing AppImage can be launched with:

```sh
build/appimage/FatalFrame-x86_64.AppImage game_files
```

The AppImage has been verified to reach the title screen with graphics and music. The latest native-runtime and FMV fixes have not all been separately verified in a newly packaged AppImage.

## Runtime compatibility

Several Xbox hardware and kernel behaviors are currently implemented through compatibility workarounds.

**DirectSound DSP acknowledgment:** Fatal Frame loads its `dsstdfx.bin` effects image and waits for commands to be acknowledged by the Xbox DSP. Until DSP execution is emulated, `RECOMP_DSP_ACK=0x85D78810` allows the guest to continue initialization. This address is specific to the supported game build and may change if its contiguous-memory allocation layout changes.

**Case-insensitive DVD filesystem:** The Linux runtime resolves Xbox disc paths without requiring matching filename capitalization. This allows requests such as `media/movie/scene9100.wmv` and `media/dsstdfx.bin` to access their corresponding files under `Media/`.

**Kernel handle duplication:** Duplicate guest handles now retain independent identities, preventing the closure of a temporary duplicate from invalidating the original handle. This is necessary for the title screen to reload its resources after an FMV finishes.

These fixes are implemented in the host and the sibling `xboxrecomp` toolkit. They are separate from the game's generated C code.

## Bring-up tools

- `kill -USR1 <pid>` prints guest-thread backtraces and register state, helping identify stalls in recompiled functions.
- `RECOMP_WATCHDOG_SECS=N` enables the runtime watchdog.
- `tools/feedback.sh [secs]` collects indirect-call targets for additional function discovery and regeneration.
- `XBOX_LOG_LEVEL=4` and `RECOMP_KERNEL_LOG_BUDGET=100000` enable detailed kernel and filesystem logging.
- `FFR_WATCH_WRITE` and `FFR_DUMP_RAM` provide optional guest-memory diagnostics where supported.

Some defaults in `src/main.c` stand in for Xbox hardware behavior that is not yet fully implemented. These should eventually be replaced by accurate hardware emulation.

## Roadmap

1. **Controller input.** Connect host keyboard and gamepad input to the emulated Xbox controller interface so the title screen, menus and gameplay become navigable.
2. **Gameplay bring-up.** Start a New Game, reach the playable environment, identify missing functions and resolve any new runtime or rendering issues.
3. **Graphics and audio.** Continue improving NV2A/Vulkan rendering, frame pacing, synchronization and APU audio. Investigate intermittent FMV audio dropouts and incomplete DSP effects emulation.
4. **FMV and game transitions.** Verify New Game and story cinematics, manual A/Start skipping, and transitions into and out of gameplay. Preserve the existing working title-screen attract loop.
5. **Saves and stability.** Verify save/load behavior, filesystem persistence, guest threading and long-running stability.
6. **Windows build.** Build and test the native Windows executable, `FatalFrame.exe`.
7. **Performance and packaging.** Improve rendering performance, shader caching, AppImage distribution and first-launch installation. Revalidate packaged builds after runtime changes.

## Disclaimer

*FATAL FRAME Recompiled* is an independent, unofficial project. It does not distribute copyrighted game assets and is not affiliated with or endorsed by Koei Tecmo or Microsoft.