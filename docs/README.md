# 🕯️ Candle (秉烛)

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Qt6](https://img.shields.io/badge/Qt-6.2+-green.svg)](https://www.qt.io/)
[![DTK6](https://img.shields.io/badge/DTK-6.0+-orange.svg)](https://github.com/linuxdeepin/dtk6widget)

🌐 **Languages**: [English](README.md) | [简体中文](README.zh-Hans.md) | [繁體中文](README.zh-Hant.md)

A DTK6/Qt6 desktop application for temporarily preventing automatic screen dimming, locking, and system sleep by simulating NumLock key presses.

## ✨ Features

- **🎛️ Service Control**: Start, stop, and restart the numlock daemon service
- **⚙️ Configuration**: Adjust the numlock simulation interval (in minutes)
- **📜 Real-time Logging**: View daemon logs in real-time
- **🔔 Lifecycle Management**: numlockd runs as the current user, started by the panel and terminated when the panel exits (no systemd service)

## 📋 Requirements

- Linux operating system
- Qt6 (>= 6.2)
- DTK6 (Deepin Toolkit 6)
- udev uaccess rule (configured by install script; relies on systemd-logind, which deepin/UOS already provide)
- `/dev/uinput` device access

## 🚀 Building

```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build
cmake --build .
```

## 📦 Installation

### Prerequisites

1. Ensure you have `/dev/uinput` access (least privilege: uaccess rule):
   ```bash
   # The install script sets up the uaccess udev rule;
   # only the active session user gets access, no need to join the input group
   ```

2. Install build dependencies:
   - Qt6 development packages
   - DTK6 development packages
   - CMake >= 3.16
   - GCC/G++ with C++17 support

### Build and Install

```bash
# Build the project
mkdir build && cd build
cmake ..
cmake --build .

# Install (as root)
sudo cmake --install .
```

### Permission Setup (udev uaccess)

Run the provided installation script:

```bash
sudo ./scripts/install.sh
```

This will:
1. Check that `/dev/uinput` exists
2. Install the uaccess udev rule (`TAG+="uaccess"`, active session user only)
3. Clean up any leftover systemd service from older versions

> **Note**: numlockd no longer runs as a systemd service. The daemon is
> started by the candle panel **as the current user** and is terminated
> automatically when the panel exits (least-privilege principle, no root process).

## 📝 Usage

1. Launch the Candle application
2. Use the control buttons to start/stop the numlock daemon
3. Adjust the interval using the spin box and click "Apply"
4. View real-time logs in the log panel

## 📁 Project Structure

```
candle/
├── src/                    # Source code
│   ├── app/               # GUI application
│   │   ├── main.cpp
│   │   ├── mainwindow.cpp
│   │   ├── mainwindow.h
│   │   └── resources.qrc
│   └── numlockd/          # Daemon source code
│       ├── main.cpp
│       ├── ipc_server.cpp
│       ├── ipc_server.h
│       ├── numlock_simulator.cpp
│       └── numlock_simulator.h
├── assets/                # Assets
│   └── images/
│       └── logo.svg
├── docs/                  # Documentation
│   ├── README.md          # English documentation
│   ├── README.zh-Hans.md  # Simplified Chinese
│   └── README.zh-Hant.md  # Traditional Chinese
├── scripts/               # Scripts
│   └── install.sh
├── config/                # udev/config files (no systemd service file)
├── CMakeLists.txt         # Build configuration
└── LICENSE.txt            # License
```

## 📄 License

This project is licensed under the GNU General Public License v3.0 (GPLv3). See [LICENSE.txt](../LICENSE.txt) for details.

## 🤝 Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

---

💡 **Tip**: "秉烛" (Bǐng Zhú) means "holding a candle" in Chinese, symbolizing keeping the light on and preventing the screen from going dark.
