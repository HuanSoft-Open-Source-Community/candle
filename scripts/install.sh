#!/bin/bash

set -e

echo "正在检查安装环境..."

if [ ! -e /dev/uinput ]; then
    echo "错误: /dev/uinput 不存在"
    exit 1
fi

echo "检查 /dev/uinput 写权限..."

if [ ! -w /dev/uinput ]; then
    echo "注意: 当前用户没有 /dev/uinput 的写权限，将安装 udev uaccess 规则"
    echo "（安装后需重新插拔/重载规则，活动会话用户即可访问；无需加入 input 组）"
fi

echo "安装 udev uaccess 规则..."

# 最小权限原则：uaccess 仅授予当前活动会话用户 /dev/uinput 访问权，
# 无需 root 运行守护进程，无需将用户加入 input 组。
echo 'KERNEL=="uinput", MODE="0660", GROUP="input", TAG+="uaccess"' \
    | sudo tee /etc/udev/rules.d/99-candle.rules > /dev/null || {
        echo "错误: 写入 udev 规则失败（sudo 失败？）" >&2
        exit 1
    }

if command -v udevadm &> /dev/null; then
    sudo udevadm control --reload-rules || { echo "警告: 重载 udev 规则失败" >&2; }
    sudo udevadm trigger || { echo "警告: 触发 udev 规则失败" >&2; }
fi

# 清理旧版本遗留的 systemd 服务（如曾用 install.sh 安装过）
if command -v systemctl &> /dev/null; then
    sudo systemctl disable --now numlockd.service 2>/dev/null || true
fi
sudo rm -f /etc/systemd/system/numlockd.service

echo ""
echo "==================================="
echo "权限配置完成！"
echo "==================================="
echo ""
echo "说明："
echo "  - numlockd 不再作为 systemd 服务运行（最小权限原则）"
echo "  - 守护进程由 candle 面板以当前用户身份拉起，面板退出即终止"
echo "  - /dev/uinput 仅当前活动会话用户可访问（uaccess）"
echo ""
echo "请重新登录（或执行 sudo udevadm trigger）后，运行 candle 面板即可。"
