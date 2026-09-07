# Desktop SDL backends

SDL3 is the default desktop backend for CMake game hosts that include
`runner/runner.cmake` and call `snesrecomp_target_sdl(<target>)`.

```sh
# Default: SDL3
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/SDL3

# Explicit compatibility fallback: SDL2
cmake -S . -B build-sdl2 -DSNESRECOMP_SDL_BACKEND=SDL2
```

The helper links the matching imported CMake target, sets the host and
recomp-ui compile definitions together, and copies the selected SDL runtime
DLL beside Windows executables. Use separate build directories when comparing
backends.

## Game benchmark mode

Super Mario World and Mega Man X accept `--benchmark <frames> <rom>`. The mode
uses the SDL renderer with VSync, frame-delay pacing, audio, autosave, and the
launcher disabled, then prints one machine-readable `SNESRECOMP_BENCHMARK`
JSON record. `--benchmark-audio` keeps audio enabled for finite callback smoke
tests; its timing should not be compared with the audio-disabled benchmark.

For a fair comparison, use Release builds from the same compiler and source
revision, close other CPU/GPU-heavy workloads, warm each executable once, and
report the median of multiple equal-length runs. The benchmark output also
reports the SDL renderer and effective VSync state.
