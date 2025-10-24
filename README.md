# Kitty DOOM

A pure C99 port of DOOM that renders directly in your terminal using the [Kitty Graphics Protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/).
Play the legendary first-person shooter in terminals that support the protocol.

Supported Terminals
- [Kitty](https://sw.kovidgoyal.net/kitty/) - Full support, best performance
- [Ghostty](https://ghostty.org) - Full support, excellent performance

## Features

- Terminal graphics rendering via [Kitty Graphics Protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/)
- Pure C99 implementation with POSIX threading
- Standard VT input sequences for keyboard control
- Cross-platform support for Linux and macOS
- Based on the [PureDOOM](https://github.com/Daivuk/PureDOOM) single-header port

## Technical Notes

### Rendering Pipeline
- Resolution: 320x200 framebuffer (classic DOOM resolution)
- Color format: RGB24 (indexed palette to RGB conversion)
- Transfer: Base64-encoded in 4KB chunks
- Protocol: Kitty Graphics Protocol with frame-by-frame transmission
- Display: First frame uses `a=T` (transmit), subsequent frames use `a=f` (frame update)

### Input System
- Threading: Dedicated pthread for input handling
- Parsing: VT sequence state machine (ground → esc → csi/ss3)
- Key behavior: Simple press-release for all keys (1ms delay for DOOM compatibility)
- Fire key: F or I keys (Ctrl is difficult to capture in terminal environments)

### Engine
- Based on [PureDOOM](https://github.com/Daivuk/PureDOOM) single-header port
- Frame rate: 35 FPS (original DOOM timing)
- No sound support (display-only implementation)

## Requirements

- C99-compatible compiler (GCC or Clang)
- GNU Make 3.81 or later
- POSIX pthread library
- curl or wget for dependency download
- Terminal with Kitty Graphics Protocol support (Kitty, Ghostty, etc.)

## Quick Start

```bash
git clone https://github.com/jserv/kitty-doom
cd kitty-doom
make
make run
```

The build system automatically downloads `PureDOOM.h` and `DOOM1.WAD` (shareware version) on first build.

## Build Targets

```bash
make                  # Build the project (downloads dependencies automatically)
make run              # Build and run the game
make download-assets  # Manually download DOOM1.WAD and PureDOOM.h
make clean            # Remove build artifacts
make distclean        # Remove all generated files including downloads
```

## Running the Game

```bash
# Basic usage
./build/kitty-doom

# Or use the convenience target
make run
```

For detailed controls and gameplay options, see [USAGE.md](USAGE.md).

## License

This project is released under GPL-2.0. See [LICENSE](LICENSE) for details.

The GPL is inherited from:
- DOOM Engine: Released by id Software under GPL-2.0 in 1997
- PureDOOM: Derived from id Software's GPL code
- This Project: As a derivative work, also under GPL-2.0

GPL ensures that DOOM and its derivatives remain free and open source.
