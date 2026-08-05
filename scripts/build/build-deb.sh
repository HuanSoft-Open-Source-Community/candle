#!/bin/bash

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 默认配置
VERSION=""
ARCH="amd64"
PACKAGE_NAME="candle"
MAINTAINER="Candle Project <candle@example.com>"
DESCRIPTION="Candle - Prevent screen dimming and system sleep"
DESCRIPTION_LONG="A DTK6/Qt6 desktop application for temporarily preventing automatic screen dimming, locking, and system sleep by simulating NumLock key presses."

# 显示帮助信息
show_help() {
    cat << EOF
用法: $(basename "$0") [选项]

构建 Candle 项目的 Debian 包

选项:
    -v, --version <version>   指定版本号 (例如: 1.0.0)
                              如果不指定，将交互式询问
    -a, --arch <arch>         指定架构: amd64 或 aarch64
                              默认: amd64
    -h, --help               显示此帮助信息

示例:
    $(basename "$0")                          # 交互式输入版本号，使用默认 amd64 架构
    $(basename "$0") -v 1.0.0                 # 指定版本号 1.0.0，使用默认 amd64 架构
    $(basename "$0") -v 1.0.0 -a aarch64      # 指定版本号 1.0.0，使用 aarch64 架构
    $(basename "$0") --version 2.0.0 --arch amd64

EOF
}

# 打印信息
info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 解析命令行参数
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -v|--version)
                VERSION="$2"
                shift 2
                ;;
            -a|--arch)
                ARCH="$2"
                shift 2
                ;;
            -h|--help)
                show_help
                exit 0
                ;;
            *)
                error "未知参数: $1"
                show_help
                exit 1
                ;;
        esac
    done
}

# 验证架构参数
validate_arch() {
    case "$ARCH" in
        amd64|aarch64)
            ;;
        *)
            error "不支持的架构: $ARCH"
            error "支持的架构: amd64, aarch64"
            exit 1
            ;;
    esac
}

# 交互式询问版本号
ask_version() {
    if [[ -z "$VERSION" ]]; then
        echo ""
        echo "请输入版本号 (例如: 1.0.0):"
        read -r VERSION
        
        if [[ -z "$VERSION" ]]; then
            error "版本号不能为空"
            exit 1
        fi
        
        # 简单验证版本号格式
        if [[ ! "$VERSION" =~ ^[0-9]+(\.[0-9]+)*$ ]]; then
            warn "版本号格式可能不正确，建议使用 x.y.z 格式"
            echo "是否继续? (y/N)"
            read -r confirm
            if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
                exit 1
            fi
        fi
    fi
}

# 检查构建依赖
check_dependencies() {
    info "检查构建依赖..."
    
    local missing_deps=()
    
    if ! command -v cmake &> /dev/null; then
        missing_deps+=("cmake")
    fi
    
    if ! command -v make &> /dev/null; then
        missing_deps+=("make")
    fi
    
    if ! command -v dpkg-deb &> /dev/null; then
        missing_deps+=("dpkg-deb")
    fi
    
    if ! command -v g++ &> /dev/null; then
        missing_deps+=("g++")
    fi
    
    if [[ ${#missing_deps[@]} -gt 0 ]]; then
        error "缺少以下依赖项:"
        for dep in "${missing_deps[@]}"; do
            echo "  - $dep"
        done
        echo ""
        echo "请安装依赖:"
        echo "  sudo apt-get install cmake make dpkg-dev g++"
        exit 1
    fi
    
    info "依赖检查通过"
}

# 获取项目根目录
get_project_root() {
    local script_dir
    script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    echo "$(cd "$script_dir/../.." && pwd)"
}

# 构建项目
build_project() {
    local project_root="$1"
    local build_dir="$2"
    
    info "开始构建项目..."
    info "构建目录: $build_dir"
    info "目标架构: $ARCH"
    
    mkdir -p "$build_dir"
    cd "$build_dir"
    
    # 设置交叉编译参数
    local cmake_args=()
    if [[ "$ARCH" == "aarch64" ]]; then
        if command -v aarch64-linux-gnu-gcc &> /dev/null; then
            cmake_args+=(-DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc)
            cmake_args+=(-DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++)
        else
            warn "未找到 aarch64 交叉编译器，尝试使用系统默认编译器"
            warn "如需交叉编译，请安装: sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu"
        fi
    fi
    
    # 配置
    info "运行 CMake 配置..."
    cmake "${cmake_args[@]}" "$project_root"
    
    # 构建
    info "编译项目..."
    cmake --build . --parallel "$(nproc)"
    
    info "项目构建完成"
}

# 创建 Debian 包目录结构
create_deb_structure() {
    local pkg_dir="$1"
    local project_root="$2"
    
    info "创建 Debian 包目录结构..."
    
    # 创建目录
    mkdir -p "$pkg_dir/DEBIAN"
    mkdir -p "$pkg_dir/opt/apps/org.yxzl.candle/bin"
    mkdir -p "$pkg_dir/usr/share/icons/hicolor/scalable/apps"
    mkdir -p "$pkg_dir/lib/systemd/system"
    mkdir -p "$pkg_dir/usr/share/applications"
    mkdir -p "$pkg_dir/usr/share/doc/candle"
    
    # 创建 control 文件
    cat > "$pkg_dir/DEBIAN/control" << EOF
Package: $PACKAGE_NAME
Version: $VERSION
Architecture: $ARCH
Maintainer: $MAINTAINER
Depends: libdtk6widget, libdtk6core, libqt6widgets6, libqt6core6, libqt6gui6, systemd
Recommends: libxcb1, libwayland-client0
Section: utils
Priority: optional
Homepage: https://github.com/yxzl/candle
Description: $DESCRIPTION
 $DESCRIPTION_LONG
EOF
    
    # 创建 postinst 脚本
    cat > "$pkg_dir/DEBIAN/postinst" << 'EOF'
#!/bin/bash
set -e

# 重新加载 systemd 配置
if command -v systemctl &> /dev/null; then
    systemctl daemon-reload
    
    # 启用服务
    systemctl enable numlockd.service || true
fi

# 设置 uinput 权限
echo 'KERNEL=="uinput", MODE="0660", GROUP="input"' > /etc/udev/rules.d/99-candle.rules

if command -v udevadm &> /dev/null; then
    udevadm control --reload-rules || true
    udevadm trigger || true
fi

# 更新图标缓存
if command -v gtk-update-icon-cache &> /dev/null; then
    gtk-update-icon-cache -f /usr/share/icons/hicolor || true
fi

exit 0
EOF
    chmod 755 "$pkg_dir/DEBIAN/postinst"
    
    # 创建 prerm 脚本
    cat > "$pkg_dir/DEBIAN/prerm" << 'EOF'
#!/bin/bash
set -e

# 停止并禁用服务
if command -v systemctl &> /dev/null; then
    systemctl stop numlockd.service || true
    systemctl disable numlockd.service || true
fi

exit 0
EOF
    chmod 755 "$pkg_dir/DEBIAN/prerm"
    
    # 创建 postrm 脚本
    cat > "$pkg_dir/DEBIAN/postrm" << 'EOF'
#!/bin/bash
set -e

# 删除 udev 规则
if [ "$1" = "purge" ]; then
    rm -f /etc/udev/rules.d/99-candle.rules
    
    if command -v udevadm &> /dev/null; then
        udevadm control --reload-rules || true
    fi
fi

exit 0
EOF
    chmod 755 "$pkg_dir/DEBIAN/postrm"
    
    # 复制构建产物
    info "复制构建产物..."
    
    # 复制可执行文件（CMake 将可执行文件输出到构建根目录）
    if [[ -f "$build_dir/candle" ]]; then
        cp "$build_dir/candle" "$pkg_dir/opt/apps/org.yxzl.candle/bin/"
        chmod 755 "$pkg_dir/opt/apps/org.yxzl.candle/bin/candle"
    else
        error "未找到 candle 可执行文件"
        exit 1
    fi
    
    if [[ -f "$build_dir/numlockd" ]]; then
        cp "$build_dir/numlockd" "$pkg_dir/opt/apps/org.yxzl.candle/bin/"
        chmod 755 "$pkg_dir/opt/apps/org.yxzl.candle/bin/numlockd"
    else
        error "未找到 numlockd 可执行文件"
        exit 1
    fi
    
    # 复制图标到系统标准图标路径
    if [[ -f "$project_root/assets/images/logo.svg" ]]; then
        cp "$project_root/assets/images/logo.svg" "$pkg_dir/usr/share/icons/hicolor/scalable/apps/org.yxzl.candle.svg"
    fi
    
    # 复制服务文件
    if [[ -f "$project_root/config/numlockd.service" ]]; then
        cp "$project_root/config/numlockd.service" "$pkg_dir/lib/systemd/system/"
    fi
    
    # 创建桌面文件
    cat > "$pkg_dir/usr/share/applications/org.yxzl.candle.desktop" << EOF
[Desktop Entry]
Name=Candle
Name[zh_CN]=秉烛
Comment=Prevent screen dimming and system sleep
Comment[zh_CN]=防止屏幕变暗和系统休眠
Exec=/opt/apps/org.yxzl.candle/bin/candle
Icon=org.yxzl.candle
Type=Application
Categories=Utility;System;
Terminal=false
StartupNotify=true
EOF
    
    # 复制版权文件
    if [[ -f "$project_root/LICENSE.txt" ]]; then
        cp "$project_root/LICENSE.txt" "$pkg_dir/usr/share/doc/candle/copyright"
        chmod 644 "$pkg_dir/usr/share/doc/candle/copyright"
    fi
    
    # 创建 changelog
    cat > "$pkg_dir/usr/share/doc/candle/changelog" << EOF
candle ($VERSION) unstable; urgency=medium

  * Release version $VERSION

 -- $MAINTAINER  $(date -R)
EOF
    gzip -9 -n -f "$pkg_dir/usr/share/doc/candle/changelog"
    
    # 设置权限
    find "$pkg_dir" -type d -exec chmod 755 {} \;
    find "$pkg_dir/usr/share/doc" -type f -exec chmod 644 {} \;
    
    info "Debian 包目录结构创建完成"
}

# 构建 Debian 包
build_deb() {
    local pkg_dir="$1"
    local output_dir="$2"
    
    info "构建 Debian 包..."
    
    mkdir -p "$output_dir"
    
    local deb_name="${PACKAGE_NAME}_${VERSION}_${ARCH}.deb"
    local deb_path="$output_dir/$deb_name"
    
    dpkg-deb --build "$pkg_dir" "$deb_path"
    
    if [[ -f "$deb_path" ]]; then
        info "Debian 包构建成功: $deb_path"
        
        # 显示包信息
        echo ""
        info "包信息:"
        dpkg-deb --info "$deb_path"
        
        echo ""
        info "包内容:"
        dpkg-deb --contents "$deb_path"
    else
        error "Debian 包构建失败"
        exit 1
    fi
}

# 主函数
main() {
    echo "======================================"
    echo "  Candle Debian 包构建脚本"
    echo "======================================"
    echo ""
    
    # 解析参数
    parse_args "$@"
    
    # 验证架构
    validate_arch
    
    # 交互式询问版本号
    ask_version
    
    echo ""
    info "配置信息:"
    echo "  版本: $VERSION"
    echo "  架构: $ARCH"
    echo ""
    
    # 检查依赖
    check_dependencies
    
    # 获取项目根目录
    PROJECT_ROOT=$(get_project_root)
    info "项目根目录: $PROJECT_ROOT"
    
    # 创建临时目录
    TEMP_DIR=$(mktemp -d)
    build_dir="$TEMP_DIR/build"
    pkg_dir="$TEMP_DIR/pkg"
    output_dir="$PROJECT_ROOT/dist"
    
    info "临时目录: $TEMP_DIR"
    
    # 清理函数
    cleanup() {
        if [[ -d "$TEMP_DIR" ]]; then
            info "清理临时文件..."
            rm -rf "$TEMP_DIR"
        fi
    }
    
    # 设置退出时清理
    trap cleanup EXIT
    
    # 构建项目
    build_project "$PROJECT_ROOT" "$build_dir"
    
    # 创建包结构
    create_deb_structure "$pkg_dir" "$PROJECT_ROOT"
    
    # 构建 deb 包
    build_deb "$pkg_dir" "$output_dir"
    
    echo ""
    echo "======================================"
    info "构建完成!"
    echo "======================================"
    echo ""
    echo "输出文件: $output_dir/${PACKAGE_NAME}_${VERSION}_${ARCH}.deb"
    echo ""
    info "安装命令:"
    echo "  sudo dpkg -i $output_dir/${PACKAGE_NAME}_${VERSION}_${ARCH}.deb"
    echo "  sudo apt-get install -f  # 如有依赖问题"
    echo ""
}

main "$@"
