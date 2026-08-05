# Candle Debian 包构建脚本使用文档

## 简介

本文档介绍如何使用 `build-deb.sh` 脚本为 Candle 项目构建 Debian 安装包。

Candle (秉烛) 是一个基于 DTK6/Qt6 的桌面应用程序，通过模拟 NumLock 按键来暂时防止屏幕自动变暗、锁定和系统休眠。

## 前置要求

### 系统要求

- Debian/Ubuntu 或兼容的 Linux 发行版
- 支持 amd64 或 aarch64 架构

### 构建依赖

在运行构建脚本之前，请确保已安装以下依赖：

```bash
sudo apt-get update
sudo apt-get install -y \
    cmake \
    make \
    dpkg-dev \
    g++ \
    qt6-base-dev \
    qt6-tools-dev \
    libdtk6widget-dev \
    libdtk6core-dev \
    libdtk6gui-dev
```

### 交叉编译依赖（可选）

如需构建 aarch64 架构的包在非 x86 设备上使用，需要安装交叉编译器：

```bash
sudo apt-get install -y \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu
```

## 使用方法

### 基本用法

```bash
./scripts/build/build-deb.sh [选项]
```

### 参数说明

| 参数 | 长参数 | 说明 | 默认值 |
|------|--------|------|--------|
| `-v` | `--version` | 指定版本号（例如：1.0.0） | 交互式询问 |
| `-a` | `--arch` | 指定目标架构：`amd64` 或 `aarch64` | `amd64` |
| `-h` | `--help` | 显示帮助信息 | - |

### 使用示例

#### 1. 交互式构建（推荐初次使用）

```bash
./scripts/build/build-deb.sh
```

脚本会提示输入版本号，然后使用默认的 `amd64` 架构进行构建。

#### 2. 指定版本号构建

```bash
./scripts/build/build-deb.sh -v 1.0.0
```

#### 3. 指定架构构建

```bash
# 构建 amd64 架构包
./scripts/build/build-deb.sh -v 1.0.0 -a amd64

# 构建 aarch64 架构包
./scripts/build/build-deb.sh -v 1.0.0 -a aarch64
```

#### 4. 使用长参数形式

```bash
./scripts/build/build-deb.sh --version 2.0.0 --arch aarch64
```

## 输出位置

构建完成后，Debian 包将生成在项目根目录的 `dist/` 文件夹中：

```
candle/
├── dist/
│   └── candle_1.0.0_amd64.deb    # 生成的安装包
├── scripts/
│   └── build/
│       └── build-deb.sh          # 构建脚本
└── ...
```

## 安装生成的包

### 安装

```bash
sudo dpkg -i dist/candle_1.0.0_amd64.deb
```

### 解决依赖问题

如果安装时出现依赖错误，运行：

```bash
sudo apt-get install -f
```

### 卸载

```bash
sudo apt-get remove candle
```

完全卸载（包括配置文件）：

```bash
sudo apt-get purge candle
```

## 包内容说明

构建的 Debian 包包含以下内容：

| 路径 | 说明 |
|------|------|
| `/opt/apps/org.yxzl.candle/bin/candle` | GUI 应用程序 |
| `/opt/apps/org.yxzl.candle/bin/numlockd` | 守护进程 |
| `/opt/apps/org.yxzl.candle/share/icons/hicolor/scalable/apps/org.yxzl.candle.svg` | 应用程序图标 |
| `/usr/share/applications/org.yxzl.candle.desktop` | 桌面启动器 |
| `/usr/share/doc/candle/` | 文档和版权信息 |

> **注意**：包中不再包含 systemd 服务文件（最小权限原则）。
> numlockd 由 candle 面板以当前用户身份拉起，面板退出即终止；
> `/dev/uinput` 权限由 postinst 安装的 uaccess udev 规则授予活动会话用户。

## 包依赖

安装包会自动处理以下依赖：

- `libdtk6widget` - DTK6 控件库
- `libdtk6core` - DTK6 核心库
- `libqt6widgets6` - Qt6 控件库
- `libqt6core6` - Qt6 核心库
- `libqt6gui6` - Qt6 GUI 库
- `systemd` - 提供 systemd-logind（uaccess udev 规则依赖；deepin/UOS 默认安装）

## 安装后配置

### uinput 权限

安装脚本会自动创建 udev 规则以允许访问 `/dev/uinput` 设备：

```
KERNEL=="uinput", MODE="0660", GROUP="input", TAG+="uaccess"
```

> `TAG+="uaccess"` 由 systemd-logind 为**当前活动会话用户**添加 ACL，
> 无需将用户加入 input 组，也无需 root 运行守护进程。

### 守护进程生命周期

包中**不包含** systemd 服务。`numlockd` 由 `candle` 面板以当前用户身份
自动拉起，面板退出时自动终止（最小权限原则，无后台 root 进程）。

启动/停止由面板控制按钮管理：

```bash
# 打开面板（自动拉起守护进程）
candle

# 停止守护进程（面板内点击"停止服务"或直接关闭面板）
```

## 故障排除

### 构建失败

1. **缺少依赖**
   
   确保已安装所有构建依赖：
   ```bash
   sudo apt-get install cmake make dpkg-dev g++
   ```

2. **CMake 配置错误**
   
   检查 Qt6 和 DTK6 开发包是否安装：
   ```bash
   sudo apt-get install qt6-base-dev libdtk6widget-dev
   ```

3. **交叉编译器未找到**
   
   构建 aarch64 包时需要安装交叉编译器：
   ```bash
   sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
   ```

### 安装后问题

1. **服务无法启动**
   
   检查 uinput 权限：
   ```bash
   ls -la /dev/uinput
   ```
   
   确保当前用户在 `input` 组：
   ```bash
   sudo usermod -aG input $USER
   ```
   
   重新登录后生效。

2. **应用程序无法启动**
   
   检查依赖是否完整：
   ```bash
   ldd /opt/apps/org.yxzl.candle/bin/candle
   ```

## 许可证

Candle 项目使用 GNU General Public License v3.0 (GPLv3) 许可证。

构建的 Debian 包会包含完整的许可证文本，位于 `/usr/share/doc/candle/copyright`。

## 更多信息

- 项目主页：https://github.com/yxzl/candle
- 问题反馈：请在项目仓库提交 Issue
- 详细文档：参见项目 `docs/` 目录下的其他文档
