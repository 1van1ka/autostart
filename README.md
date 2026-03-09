Autostart Launcher
========================
A lightweight C program that scans and launches applications from XDG autostart directories (.desktop files) with proper dependency checking and staggered startup delays.
___
## Features

- **Cross-platform XDG compliance** - Supports both user (`~/.config/autostart`) and system-wide (`/etc/xdg/autostart`) autostart directories
- **Filtering** - Automatically skips hidden, no-display, and invalid desktop entries
- **Staggered startup** - Launches applications with configurable delays to prevent system overload
- **Background execution** - Applications run detached from terminal in their own session
- **Resource efficient** - Minimal memory footprint, no unnecessary dependencies

## Installation
### Prerequisites

- C compiler
- POSIX-compliant system (Linux, BSD)

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
[INFO] === Current Config =====================
[INFO] Startup delay: 0 ms
[INFO] Delay between apps: 238 ms
[INFO] Log level: 0
[INFO] Applications rules (1):
[INFO] 	- Throne.desktop: BLOCK with delay: 0
[INFO] ========================================

[INFO] [Directory 1] Scanning: /home/ivanika/.config/autostart
[INFO] 	Skipped (disallowed by config): Throne.desktop
[INFO] 	--- Summary for /home/ivanika/.config/autostart ---
[INFO] 	Queued for launch: 0 of 1 founded

[INFO] [Directory 2] Scanning: /etc/xdg/autostart
[INFO] 	Skipped (nodisplay): xfce-polkit-gnome-authentication-agent-1.desktop
[INFO] 	Queued: blueman.desktop
[INFO] 	Queued: xfce4-volumed-pulse.desktop
[INFO] 	Skipped (nodisplay): nm-applet.desktop
[INFO] 	Queued: xfce4-notifyd.desktop
[INFO] 	Skipped (nodisplay): at-spi-dbus-bus.desktop
[INFO] 	Queued: xfce4-power-manager.desktop
[INFO] 	Skipped (nodisplay): xfce4-screensaver.desktop
[INFO] 	Skipped (hidden): xfce4-clipman-plugin-autostart.desktop
[INFO] 	Queued: xfsettingsd.desktop
[INFO] 	--- Summary for /etc/xdg/autostart ---
[INFO] 	Queued for launch: 5 of 10 founded

[INFO] Launching 5 apps with 238ms delay
[INFO] 	[1/5] Access blueman.desktop
[INFO] 	[2/5] Access xfce4-volumed-pulse.desktop
[INFO] 	[3/5] Access xfce4-notifyd.desktop
[INFO] 	[4/5] Access xfce4-power-manager.desktop
[INFO] 	[5/5] Access xfsettingsd.desktop
Launch completed (5 of 5)
```

## License

This project is released under the MIT License. See LICENSE file for details.

## Acknowledgments

- Based on the [XDG Autostart Specification](https://specifications.freedesktop.org/autostart-spec/autostart-spec-latest.html)
- Inspired by various desktop environment autostart implementations
