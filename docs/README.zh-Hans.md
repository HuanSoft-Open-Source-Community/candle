# 🕯️ 秉烛 (Candle)

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Qt6](https://img.shields.io/badge/Qt-6.2+-green.svg)](https://www.qt.io/)
[![DTK6](https://img.shields.io/badge/DTK-6.0+-orange.svg)](https://github.com/linuxdeepin/dtk6widget)

🌐 **语言**: [English](README.md) | [简体中文](README.zh-Hans.md) | [繁體中文](README.zh-Hant.md)

一个基于 DTK6/Qt6 开发的桌面应用程序，用于临时阻止自动息屏、锁屏和系统休眠，通过模拟数字锁定（NumLock）按键实现。

## ✨ 功能特性

- **🎛️ 服务控制**：启动、停止、重启数字锁定守护进程
- **⚙️ 参数配置**：调节数字锁定模拟间隔（分钟）
- **📜 实时日志**：在面板中查看守护进程实时日志
- **🔔 生命周期管理**：numlockd 由面板以当前用户身份拉起，面板退出即终止（无 systemd 服务）

## 📋 环境要求

- Linux 操作系统
- Qt6 (>= 6.2)
- DTK6 (Deepin Toolkit 6)
- udev uaccess 规则（安装脚本配置，无需 systemd）
- `/dev/uinput` 设备访问权限

## 🚀 编译步骤

```bash
# 创建构建目录
mkdir build && cd build

# CMake 配置
cmake ..

# 编译
cmake --build .
```

## 📦 安装指南

### 准备工作

1. 确保拥有 `/dev/uinput` 访问权限（最小权限：uaccess 规则）：
   ```bash
   # 安装 udev uaccess 规则（见下方安装脚本），
   # 仅当前活动会话用户可访问 /dev/uinput，无需加入 input 组
   ```

2. 安装编译依赖：
   - Qt6 开发包
   - DTK6 开发包
   - CMake >= 3.16
   - GCC/G++ 需支持 C++17

### 构建与安装

```bash
# 构建项目
mkdir build && cd build
cmake ..
cmake --build .

# 安装（需要 root 权限）
sudo cmake --install .
```

### 权限配置（udev uaccess）

运行安装脚本：

```bash
sudo ./scripts/install.sh
```

该脚本将执行以下操作：
1. 检查 `/dev/uinput` 是否存在
2. 安装 uaccess udev 规则（`TAG+="uaccess"`，仅活动会话用户可写）
3. 清理旧版本遗留的 systemd 服务（如有）

> **说明**：numlockd 不再以 systemd 服务运行。守护进程由 candle 面板
> 以**当前用户身份**拉起，面板退出即自动终止（最小权限原则，无 root 进程）。

## 📝 使用方法

### 基本使用

1. 启动 Candle 应用程序
2. 使用控制按钮启动/停止数字锁定守护进程
3. 调节数值输入框中的数值，点击"应用"按钮使配置生效
4. 在日志面板中查看实时日志

### 智能阻止（v1.1+）

智能模式可根据当前活动窗口自动判断是否阻止熄屏：

1. 打开「智能阻止」开关
2. 在「目标进程」输入框中填写进程名（如 `firefox,chromium`）
3. 当目标进程的窗口处于**活动状态且未最小化**时，守护进程才会模拟 NumLock
4. 切换窗口或最小化后自动停止阻止
5. 可选：打开「自动同步熄屏时间」以自动读取系统熄屏设置，间隔设为「熄屏时间 − 1 秒」

**平台支持**：

| 平台 | 活动窗口检测 | 自动熄屏读取 |
|------|------------|------------|
| deepin 23 | X11 EWMH | DConfig |
| deepin 25 | Treeland 私有协议 | DConfig |
| UOS 20   | X11 EWMH | GSettings |
| UOS 25   | X11 EWMH | DConfig |

> deepin 25 的 Treeland 检测需要 `treeland-protocols` 包。

## 📁 项目结构

```
candle/
├── src/                    # 源代码
│   ├── app/               # 图形界面应用程序
│   │   ├── main.cpp
│   │   ├── mainwindow.cpp
│   │   ├── mainwindow.h
│   │   └── resources.qrc
│   └── numlockd/          # 守护进程源码
│       ├── main.cpp
│       ├── ipc_server.cpp
│       ├── ipc_server.h
│       ├── numlock_simulator.cpp
│       └── numlock_simulator.h
├── assets/                # 资源文件
│   └── images/
│       └── logo.svg
├── docs/                  # 文档目录
│   ├── README.md          # 英文文档
│   ├── README.zh-Hans.md  # 简体中文
│   └── README.zh-Hant.md  # 繁体中文
├── scripts/               # 脚本
│   └── install.sh
├── config/                # 配置文件
│   └── numlockd.service
├── CMakeLists.txt         # 构建配置
└── LICENSE.txt            # 许可证
```

## 📄 许可证

本项目采用 GNU 通用公共许可证第三版 (GPLv3) 进行授权。详细信息请参阅 [LICENSE.txt](../LICENSE.txt)。

## 🤝 贡献

欢迎贡献代码！请随意提交 Issues 和 Pull Requests。

---

💡 **提示**："秉烛"寓意手持蜡烛，象征着保持屏幕常亮，防止进入息屏状态。
