# Contributing to LinuPaint

Thanks for helping! A few rules keep the project healthy.

## Before you start

- The goal of v1.x is parity with the Paint of Windows XP; phase 2 targets Windows 7. Read
  [PRD.md](PRD.md) before proposing features: layers, filters and similar tools are out of scope.
- Never copy icons, sounds, text or code from Microsoft products. Behavior is reproduced by
  observation only.

## Contributor License Agreement

Before your first pull request is merged, the CLA Assistant bot will ask you to accept the
[Contributor License Agreement](docs/CLA.md). It keeps the project able to change its license or
offer commercial builds in the future. You keep the copyright of your work.

## Code

- C++20. `src/raster` and `src/core` must not depend on Qt; keep them covered by unit tests.
- Follow the existing style (`.clang-format`).
- User-visible strings go through `tr()` in US English; update the `.ts` files with `lupdate`.
- Run the full test suite before opening a pull request:
  `QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`.
