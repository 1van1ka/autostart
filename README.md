README for project autostart
========================
# Autostart Launcher

A lightweight C program that scans and launches applications from XDG autostart directories (.desktop files) with proper dependency checking and staggered startup delays.
___
## Features

- **Cross-platform XDG compliance** - Supports both user (`~/.config/autostart`) and system-wide (`/etc/xdg/autostart`) autostart directories
- **Filtering** - Automatically skips hidden, no-display, and invalid desktop entries
- **Staggered startup** - Launches applications with configurable delays to prevent system overload
- **Background execution** - Applications run detached from terminal in their own session
- **Thread-safe design** - Uses mutex locks for thread-safe application queuing
- **Resource efficient** - Minimal memory footprint, no unnecessary dependencies

## Installation
### Prerequisites

- GCC compiler
- POSIX-compliant system (Linux, BSD)
- Standard C library with pthread support

### Compilation

```bash
# Clone or download the source code
git clone https://github.com/1van1ka/autostart.git
cd autostart

# Compile the program
make

# To instalation System Path
make install
```

## Usage

### Basic Usage

```bash
# Run directly
/path/to/autostart

# If installed system-wide
autostart
```

### Integration with Display Managers

Add to your `.xinitrc` or display manager startup script:

```bash
# In ~/.xinitrc (for startx) or with WM
autostart &
```

### Configuration

In future will be used configuration file  
Edit the source code to customize behavior:

| Constant | Default | Description |
|----------|---------|-------------|
| `DELAY_MS` | 200 | Delay between application launches (milliseconds) |

## Desktop File Support

The launcher fully supports the [XDG Desktop Entry Specification](https://specifications.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html).

### Supported Keys
- `Name` - Application display name
- `Exec` - Command to execute (with desktop specifier removal)
- `TryExec` - Executable to test for existence
- `Path` - Working directory
- `Icon` - Icon name (for reference only)
- `Terminal` - Boolean (parsed but not used for launching)
- `Hidden` - Boolean (skips if true)
- `NoDisplay` - Boolean (skips if true)

## Example Output
```
Autostart Launcher
=============================================
Configuration:
  Delay between application starts: 200ms
  Maximum applications: 100

Scanning directories:
  1. /home/ivanika/.config/autostart
  2. /etc/xdg/autostart
  3. /usr/share/autostart


[Directory 1] Scanning: /home/ivanika/.config/autostart
  Queued: Discord
  Queued: Telegram
  Queued: NetworkManager Applet

  --- Summary for /home/ivanika/.config/autostart ---
  Total .desktop files found: 3
  Queued for launch: 3
  Skipped: 0

[Directory 2] Scanning: /etc/xdg/autostart
  Skipped (hidden/no-display): AT-SPI D-Bus Bus
  Skipped (hidden/no-display): User folders update
  Skipped (hidden/no-display): NetworkManager Applet

  --- Summary for /etc/xdg/autostart ---
  Total .desktop files found: 3
  Queued for launch: 0
  Skipped: 3

Warning: Autostart directory does not exist: /usr/share/autostart

========================================
Launching 3 applications with 200ms delay
========================================
Thread 0: Launching: Discord
Thread 1: Launching: Telegram
Thread 2: Launching: NetworkManager Applet
All launch threads initiated

========================================
All autostart applications processed.
========================================
```

## Performance

- **Memory usage**: >=0.5 MB resident memory
- **CPU usage**: Minimal, only during startup scanning
- **Startup time**: >= 0.1 second for typical autostart directories

## License

This project is released under the MIT License. See LICENSE file for details.

## Acknowledgments

- Based on the [XDG Autostart Specification](https://specifications.freedesktop.org/autostart-spec/autostart-spec-latest.html)
- Inspired by various desktop environment autostart implementations
- Uses POSIX threading for parallel application launching