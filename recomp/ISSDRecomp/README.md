# International Superstar Soccer Deluxe (USA) recompilation project

Generated locally from your ROM by snesrecomp.

## Build the generated source

Install CMake, Ninja, and a C compiler. On Windows, run:

```powershell
.\build.ps1
```

On macOS or Linux, run `sh build.sh`.

**Expected build result:** a static library named `snesrecomp_game`, not a
playable executable. The library contains the automatically discovered
recompiled code. The original ROM is not copied into this project.

## Continue the port

An arbitrary SNES game still needs game-specific function boundaries,
indirect-dispatch configuration, and a host application before it is a
playable native port. Add those declarations under `config/`, regenerate the
source, and integrate the library with the runner under `snesrecomp/runner`.

`generated/` is derived from copyrighted ROM data. Do not redistribute it
unless you have permission.
