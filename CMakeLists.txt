cmake_minimum_required(VERSION 3.16)

# 项目名称
project(candle)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

# 设置安装前缀为 /opt/apps/org.yxzl.candle
set(CMAKE_INSTALL_PREFIX "/opt/apps/org.yxzl.candle" CACHE PATH "Installation prefix" FORCE)

# 查找 Qt6 包
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)

# 查找 DTK6 包
find_package(Dtk6 REQUIRED COMPONENTS Widget Core Gui)

# 添加子目录
add_subdirectory(src/app)
add_subdirectory(src/numlockd)

# 安装图标
install(FILES assets/images/logo.svg
    DESTINATION share/icons/hicolor/scalable/apps
    RENAME org.yxzl.candle.svg
)

# 说明：不再安装 systemd 服务文件（最小权限原则）。
# numlockd 由 candle GUI 以当前用户身份拉起，GUI 退出即终止；
# /dev/uinput 权限由 udev uaccess 规则（安装脚本）授予活动会话用户。
