# Architecture & Design Decisions Log

This document records major technical and architectural decisions for **ISSD Native**.

---

## ADR-001: Selection of Primary Recompilation Framework

- **Date:** 2026-09-05
- **Status:** Accepted
- **Context:**
  The project requires a high-fidelity static recompiler and runtime environment for SNES 65816 machine code and hardware subsystems (PPU, APU/SPC700, DMA, Timers).
- **Options Considered:**
  1. mstan/snesrecomp (Primary): Features 65816 static analysis, M/X register state tracking, indirect dispatch handling, portable C emission, SNES hardware runtime, differential cosimulation, and modding / widescreen foundations.
  2. sp00nznet/snesrecomp: LakeSnes-based runtime alternative.
  3. Custom from-scratch 65816-to-C recompiler.
- **Decision:**
  Adopt mstan/snesrecomp as the primary framework. Game-specific configuration and symbols will be decoupled from the core framework.
- **Tradeoffs & Consequences:**
  - Fast-tracks baseline recompilation.
  - Requires pinning upstream commit once stable to prevent unexpected breakages.
  - Runtime interpreter fallback provides high compatibility during bring-up while static coverage is iteratively expanded.

---

## ADR-002: Dual-Mode Architecture (Classic Mode vs. Enhanced Mode)

- **Date:** 2026-09-05
- **Status:** Accepted
- **Context:**
  We must balance perfect historical preservation with modern enhancements (slowdown fixes, widescreen, high-refresh, modding).
- **Decision:**
  Maintain **Classic Mode** as the authoritative behavioral and competitive baseline (original timing, original bugs, pixel-perfect 4:3 SNES rendering) and **Enhanced Mode** for modern presentation (60Hz stable simulation decoupled from high-refresh renderers, widescreen, modding).
- **Consequences:**
  - Automated differential testing and replay systems can run in Classic Mode to ensure zero unintended gameplay divergence.

---

## ADR-003: Strict Copyright & Asset Isolation

- **Date:** 2026-09-05
- **Status:** Accepted
- **Context:**
  The project must not redistribute copyrighted game binaries or assets.
- **Decision:**
  Enforce strict .gitignore filters against ROMs (*.sfc, *.smc, *.bin). ROM verification (	ools/verify_rom.py) validates clean USA dumps (MD5 345ddedcd63412b9373dabb67c11fc05) locally on the user's machine at build/run time.
