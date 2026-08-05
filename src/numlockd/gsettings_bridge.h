#ifndef GSETTINGS_BRIDGE_H
#define GSETTINGS_BRIDGE_H

/**
 * @brief GSettings 隔离桥接层
 *
 * glib 头文件（gio/gio.h）与 Qt 头文件在同一翻译单元会冲突：
 * gdbusintrospection.h 的成员名 `signals` 会被 Qt 的 `signals` 宏展开破坏。
 * 本桥接文件**不包含任何 Qt 头**，将 GSettings 访问封装为 extern "C" 接口，
 * 供 C++ 侧（power_settings_reader）调用，彻底隔离宏冲突。
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建 GSettings 句柄
 * @param schema GSettings schema 名（如 "com.deepin.dde.power"）
 * @return 句柄指针；schema 不存在时返回 nullptr
 */
void *gsettings_bridge_new(const char *schema);

/**
 * @brief 读取整数 key
 * @param handle gsettings_bridge_new 返回的句柄
 * @param key key 名
 * @return 值；handle 无效时返回 -1
 */
int gsettings_bridge_get_int(void *handle, const char *key);

/**
 * @brief 设置"任意 key 变化"回调
 * @param handle 句柄
 * @param cb 回调（userdata + key 名）
 * @param userdata 透传给回调的用户数据
 */
void gsettings_bridge_set_changed_callback(void *handle,
                                           void (*cb)(void *userdata, const char *key),
                                           void *userdata);

/**
 * @brief 释放句柄
 */
void gsettings_bridge_free(void *handle);

#ifdef __cplusplus
}
#endif

#endif // GSETTINGS_BRIDGE_H
