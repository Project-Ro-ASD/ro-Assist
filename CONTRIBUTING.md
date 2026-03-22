# Contributing to ro-Assist

Thanks for your interest in contributing.

## Development Setup

1. Install dependencies on Fedora:
   ```bash
   sudo dnf install cmake gcc-c++ qt6-qtbase-devel
   ```
2. Configure and build:
   ```bash
   cmake -S . -B build
   cmake --build build
   ```
3. Run:
   ```bash
   ./build/ro-assist
   ```

## Contribution Rules

- Keep changes focused and small.
- Follow existing C++/Qt coding style in the repository.
- Update documentation when behavior changes.
- Make sure the project still builds before opening a PR.

## Pull Request Process

1. Create a branch from `main`.
2. Describe the problem and your solution clearly.
3. Include steps to test your changes.
4. Link related issues if available.
