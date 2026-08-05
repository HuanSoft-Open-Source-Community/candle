# 🕯️ 秉燭 (Candle)

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Qt6](https://img.shields.io/badge/Qt-6.2+-green.svg)](https://www.qt.io/)
[![DTK6](https://img.shields.io/badge/DTK-6.0+-orange.svg)](https://github.com/linuxdeepin/dtk6widget)

🌐 **語言**: [English](README.md) | [简体中文](README.zh-Hans.md) | [繁體中文](README.zh-Hant.md)

DTK6/Qt6 桌面應用，用以控御數字鎖定守護進程服務，暫時阻止自動息屏、鎖屏與系統休眠。

## ✨ 功用

- **🎛️ 服務啟停**：啟、止、重啟數字鎖定守護進程
- **⚙️ 參數配置**：調節數字鎖定模擬間隔（以分鐘計）
- **📜 實時日誌**：於面板中覽守護進程之日誌
- **🔔 生命週期管理**：numlockd 由面板以當前用戶身分拉起，面板退出即終止（無 systemd 服務）

## 📋 所需環境

- Linux 操作系統
- Qt6 (>= 6.2)
- DTK6 (Deepin Toolkit 6)
- udev uaccess 規則（由安裝腳本配置，無需 systemd）
- `/dev/uinput` 設備訪問權限

## 🚀 編譯之法

```bash
# 創建構建目錄
mkdir build && cd build

# CMake 配置
cmake ..

# 編譯
cmake --build .
```

## 📦 安裝之法

### 事前準備

1. 確認擁有 `/dev/uinput` 訪問權限（最小權限：uaccess 規則）：
   ```bash
   # 由安裝腳本配置 uaccess udev 規則，
   # 僅當前活動會話用戶可訪問 /dev/uinput，無需加入 input 組
   ```

2. 安裝編譯依賴：
   - Qt6 開發包
   - DTK6 開發包
   - CMake >= 3.16
   - GCC/G++ 需支持 C++17

### 構建與安裝

```bash
# 構建項目
mkdir build && cd build
cmake ..
cmake --build .

# 安裝（需 root 權限）
sudo cmake --install .
```

### 權限配置（udev uaccess）

運行安裝腳本：

```bash
sudo ./scripts/install.sh
```

此腳本將：
1. 檢查 `/dev/uinput` 是否存在
2. 安裝 uaccess udev 規則（`TAG+="uaccess"`，僅活動會話用戶可寫）
3. 清理舊版本遺留的 systemd 服務（如有）

> **說明**：numlockd 不再以 systemd 服務運行。守護進程由 candle 面板
> 以**當前用戶身分**拉起，面板退出即自動終止（最小權限原則，無 root 進程）。

## 📝 使用之法

1. 啟動 Candle 應用
2. 使用控制按鈕啟停數字鎖定守護進程
3. 調節數值輸入框中之數值，點擊"應用"以生效
4. 於日誌面板中觀實時日誌

## 📁 項目結構

```
candle/
├── src/                    # 源碼
│   ├── app/               # 圖形界面應用程序
│   │   ├── main.cpp
│   │   ├── mainwindow.cpp
│   │   ├── mainwindow.h
│   │   └── resources.qrc
│   └── numlockd/          # 守護進程源碼
│       ├── main.cpp
│       ├── ipc_server.cpp
│       ├── ipc_server.h
│       ├── numlock_simulator.cpp
│       └── numlock_simulator.h
├── assets/                # 資源文件
│   └── images/
│       └── logo.svg
├── docs/                  # 文檔目錄
│   ├── README.md          # 英文文檔
│   ├── README.zh-Hans.md  # 簡體中文
│   └── README.zh-Hant.md  # 繁體中文
├── scripts/               # 腳本
│   └── install.sh
├── config/                # 配置文件
│   └── numlockd.service
├── CMakeLists.txt         # 構建配置
└── LICENSE.txt            # 許可證
```

## 📄 許可證

本項目採用 GNU 通用公共許可證第三版 (GPLv3) 授權。詳見 [LICENSE.txt](../LICENSE.txt)。

## 🤝 貢獻

歡迎貢獻！請隨意提交 Issues 與 Pull Requests。

---

💡 **提示**："秉燭"寓意手持蠟燭，象徵著保持屏幕常亮，防止進入息屏狀態。
