# SDL2 vs SDL3 benchmark - 2026-07-28

This is an uncapped renderer/host throughput comparison, not a 60 FPS gameplay
test. Both backends sustain far more than the required frame rate.

## Environment and method

- Windows 10 Home 19045
- Intel Core i9-9900K (8 cores / 16 threads)
- NVIDIA GeForce RTX 3080 Ti, driver 32.0.16.1074
- MinGW GCC 15.2.0, CMake Release builds
- SDL 3.4.12 (`direct3d11`) and SDL 2.32.10 (`direct3d`)
- SDL renderer, VSync off, audio off, frame-delay pacing off, autosave off
- One 300-frame warm-up per executable
- Seven alternating 1,200-frame runs from game boot; median reported

## Results

| Game | SDL2 median | SDL3 median | SDL3 frame-time change |
|---|---:|---:|---:|
| Super Mario World | 3.015238 ms / 331.649 FPS | 3.625170 ms / 275.849 FPS | +20.23% |
| Mega Man X | 2.051483 ms / 487.452 FPS | 2.870878 ms / 348.325 FPS | +39.94% |

Lower frame time is better. SDL3 is slower in this uncapped workload, but even
the slowest median is about 4.6 times faster than the 16.667 ms budget for
60 FPS gameplay.

These results were collected after explicitly setting the streaming texture to
opaque blending. Earlier pre-fix measurements rendered a black/transparent
frame under SDL3 and are not representative.

## Raw frame times (ms)

| Trial | SMW SDL2 | SMW SDL3 | MMX SDL2 | MMX SDL3 |
|---:|---:|---:|---:|---:|
| 1 | 2.680118 | 3.375091 | 2.057796 | 4.708928 |
| 2 | 3.920581 | 5.330919 | 2.026731 | 2.929146 |
| 3 | 2.514033 | 3.120737 | 2.187728 | 3.063775 |
| 4 | 3.022720 | 3.976219 | 2.134267 | 2.403047 |
| 5 | 3.096591 | 3.280332 | 2.051483 | 2.870878 |
| 6 | 3.015238 | 3.752288 | 1.986808 | 2.716685 |
| 7 | 2.856110 | 3.625170 | 2.005734 | 2.475313 |

The finite `--benchmark-audio` smoke mode was also run for 600 frames on both
games and both backends. All four runs opened their audio device, reached the
first mixer callback, and exited cleanly.
