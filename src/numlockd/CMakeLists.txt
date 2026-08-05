cmake_minimum_required(VERSION 3.16)

project(numlockd VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network DBus)
find_package(Dtk6 REQUIRED COMPONENTS Core)

include(CheckIncludeFileCXX)
check_include_file_cxx(linux/uinput.h HAVE_LINUX_UINPUT_H)
if(NOT HAVE_LINUX_UINPUT_H)
    message(FATAL_ERROR "linux/uinput.h not found")
endif()

# ---- 可选依赖：xcb（X11 活动窗口检测） ----
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(XCB xcb)
    if(XCB_FOUND)
        add_definitions(-DHAVE_XCB)
        message(STATUS "numlockd: xcb found — X11WindowMonitor enabled")
    else()
        message(WARNING "numlockd: xcb not found — X11WindowMonitor disabled")
    endif()
else()
    message(WARNING "numlockd: PkgConfig not found — X11WindowMonitor disabled")
endif()

# ---- 可选依赖：gio（GSettings，UOS 20 兜底） ----
if(PkgConfig_FOUND)
    pkg_check_modules(GIO gio-2.0)
    if(GIO_FOUND)
        add_definitions(-DHAVE_GIO)
        message(STATUS "numlockd: gio-2.0 found — GSettings fallback enabled")
    else()
        message(WARNING "numlockd: gio-2.0 not found — GSettings fallback disabled")
    endif()
endif()

# ---- 可选依赖：wayland-client（Treeland 活动窗口检测） ----
if(PkgConfig_FOUND)
    pkg_check_modules(WAYLAND_CLIENT wayland-client)
    if(WAYLAND_CLIENT_FOUND)
        add_definitions(-DHAVE_WAYLAND)
        message(STATUS "numlockd: wayland-client found — TreelandWindowMonitor enabled")
    else()
        message(WARNING "numlockd: wayland-client not found — TreelandWindowMonitor disabled")
    endif()
endif()

add_executable(numlockd)

set_target_properties(numlockd PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_sources(numlockd PRIVATE
    main.cpp
    numlock_simulator.cpp
    numlock_simulator.h
    ipc_server.cpp
    ipc_server.h
    # --- 活动窗口检测模块 ---
    window_monitor.h
    window_monitor.cpp
    x11_window_monitor.h
    x11_window_monitor.cpp
    treeland_window_monitor.h
    treeland_window_monitor.cpp
    # --- 电源设置与智能阻止 ---
    power_settings_reader.h
    power_settings_reader.cpp
    smart_blocker.h
    smart_blocker.cpp
)

# ---- GSettings 桥接层（仅 GIO 可用时编译；不包含 Qt 头，隔离 glib/Qt 宏冲突） ----
if(GIO_FOUND)
    target_sources(numlockd PRIVATE
        gsettings_bridge.h
        gsettings_bridge.cpp
    )
endif()

target_link_libraries(numlockd PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::DBus
    Dtk6::Core
)

# ---- 条件链接 xcb ----
if(XCB_FOUND)
    target_include_directories(numlockd PRIVATE ${XCB_INCLUDE_DIRS})
    target_link_libraries(numlockd PRIVATE ${XCB_LIBRARIES})
endif()

# ---- 条件链接 gio ----
if(GIO_FOUND)
    target_include_directories(numlockd PRIVATE ${GIO_INCLUDE_DIRS})
    target_link_libraries(numlockd PRIVATE ${GIO_LIBRARIES})
endif()

# ---- 条件链接 wayland-client ----
if(WAYLAND_CLIENT_FOUND)
    target_include_directories(numlockd PRIVATE ${WAYLAND_CLIENT_INCLUDE_DIRS})
    target_link_libraries(numlockd PRIVATE ${WAYLAND_CLIENT_LIBRARIES})
endif()

install(TARGETS numlockd
    RUNTIME DESTINATION bin
)
