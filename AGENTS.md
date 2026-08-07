# AGENTS.md

## Project Overview

This project implements a custom memory allocator for embedded systems.

The primary target platform is:

* Infineon AURIX TC397XA microcontroller.
* TriCore architecture.
* HighTec GNU toolchain with `tricore-gcc` (available only on Microsoft Windows).

The allocator is intended to be:

* independent from libc;
* suitable for deterministic embedded environments;
* portable enough to run on a Windows 10 host PC for algorithm development and debugging;
* reusable as a standalone memory management component.

The long-term integration target is an embedded radar tracking algorithm. The PC build exists primarily to simplify development, debugging, testing, and profiling without requiring frequent firmware deployment.

---

# Core Development Principles

## Embedded-first design

Always consider the embedded target as the primary environment.

Do not introduce solutions that depend on:

* operating system services;
* dynamic runtime features unavailable on embedded targets;
* hidden heap allocation;
* non-deterministic behavior;
* large memory overhead.

A solution that works on a PC but cannot be realistically deployed on AURIX is considered incorrect.

---

## Portability requirements

The allocator must support two environments:

1. Embedded target:

   * Infineon AURIX TC397XA;
   * TriCore architecture;
   * HighTec `tricore-gcc`.

2. Host development environment:

   * Windows 10;
   * native compilation for testing and debugging.

Platform-specific code must be isolated.

Avoid scattering architecture-dependent code throughout the allocator implementation.

Prefer clear abstraction layers:

* platform-independent allocator core;
* platform-specific memory initialization and hardware integration.

---

# Language and Toolchain Requirements

## Programming language

Use only:

* C99 standard.

Do not use:

* C++;
* compiler-specific language extensions unless absolutely required;
* non-standard libraries.

When compiler-specific features are necessary for TriCore support, isolate them and clearly document the reason.

---

## Compiler

The embedded compiler is:

* HighTec GNU TriCore compiler (`tricore-gcc`).

When adding compiler-specific code:

* use conditional compilation;
* avoid affecting host builds;
* document assumptions about compiler behavior.

Example:

```c
#ifdef TRICORE_TARGET
/* TriCore-specific implementation */
#endif
```

Do not make the entire allocator dependent on the TriCore compiler.

---

## Build system

Use only:

* GNU Make.

Do not introduce:

* CMake;
* Meson;
* platform-specific IDE build systems.

The Makefile should support:

* embedded target build;
* host PC build;
* debug build;
* release build.

---

# Memory Allocator Requirements

## No libc dependency

The allocator must not depend on:

* malloc;
* calloc;
* realloc;
* free;
* libc heap implementation.

The allocator must provide its own memory management logic.

External memory should be provided through explicit memory regions/buffers.

---

## Deterministic behavior

Prefer predictable execution time.

Avoid algorithms with uncontrolled runtime complexity.

Consider:

* fragmentation;
* allocation latency;
* deallocation latency;
* memory overhead;
* worst-case behavior.

For embedded usage, predictability is more important than maximum throughput.

---

## Memory ownership

Clearly define:

* who owns allocated memory;
* lifetime rules;
* initialization requirements;
* deinitialization behavior.

Avoid hidden ownership transfers.

---

## Error handling

Do not use:

* exceptions;
* abort;
* hidden global error states.

Prefer explicit error reporting:

* return codes;
* status structures;
* explicit failure handling.

---

# Architecture Guidelines

Keep the allocator modular.

Prefer separation between:

## Core allocator logic

Contains:

* allocation algorithms;
* block management;
* metadata handling;
* fragmentation management.

Should not know about:

* MCU registers;
* interrupts;
* hardware memory layout.

---

## Platform layer

Contains:

* memory region definition;
* startup initialization;
* linker integration;
* cache or alignment handling if required.

---

## Test layer

The host build should allow:

* unit testing;
* stress testing;
* allocation pattern testing;
* debugging with standard tools.

---

# Coding Style

Follow strict C99 style.

Prefer:

* explicit types;
* clear naming;
* small functions;
* minimal hidden state.

Avoid:

* unnecessary macros;
* deeply nested logic;
* clever optimizations without measurement.

Readable and maintainable code is preferred over premature optimization.

---

# Documentation Requirements

For non-obvious decisions document:

* why a design was chosen;
* memory trade-offs;
* timing implications;
* embedded-specific constraints.

Especially document:

* allocation algorithms;
* metadata layout;
* alignment requirements;
* fragmentation handling;
* assumptions about hardware.

---

# Testing Requirements

Every significant allocator change should consider:

* allocation/deallocation correctness;
* memory boundary conditions;
* fragmentation scenarios;
* alignment correctness;
* exhaustion behavior;
* repeated allocation patterns.

The host build should be used whenever possible for fast verification before embedded deployment.

---

# Change Guidelines

Before modifying allocator architecture:

* understand existing memory model;
* identify impact on embedded target;
* preserve host-build compatibility.

Do not introduce dependencies without strong justification.

When proposing a significant change, explain:

1. Why the current approach is insufficient.
2. What trade-offs the new approach introduces.
3. How the change affects embedded constraints.

---

# Working Style

When analyzing problems:

* prioritize correctness and determinism;
* consider embedded constraints first;
* explain trade-offs explicitly;
* avoid assuming the availability of an operating system or standard runtime.

When uncertain about hardware/compiler behavior:

* identify the assumption;
* suggest verification steps;
* avoid silently relying on undocumented behavior.
