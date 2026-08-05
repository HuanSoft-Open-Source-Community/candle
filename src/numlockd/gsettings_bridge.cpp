// 本文件**不包含任何 Qt 头**，避免 glib 与 Qt 的宏冲突（signals 等）。
// 由 power_settings_reader.cpp 通过 extern "C" 接口调用。

#include "gsettings_bridge.h"

#include <gio/gio.h>

struct BridgeHandle {
    GSettings *settings = nullptr;
    void (*cb)(void *, const char *) = nullptr;
    void *userdata = nullptr;
};

static void onGSettingsChanged(GSettings * /*settings*/, const char *key, gpointer userData)
{
    auto *h = static_cast<BridgeHandle *>(userData);
    if (h && h->cb) {
        h->cb(h->userdata, key);
    }
}

void *gsettings_bridge_new(const char *schema)
{
    if (!schema) return nullptr;
    GSettings *s = g_settings_new(schema);
    if (!s) return nullptr;

    auto *h = new BridgeHandle;
    h->settings = s;
    g_signal_connect(s, "changed", G_CALLBACK(onGSettingsChanged), h);
    return h;
}

int gsettings_bridge_get_int(void *handle, const char *key)
{
    auto *h = static_cast<BridgeHandle *>(handle);
    if (!h || !h->settings || !key) return -1;
    return g_settings_get_int(h->settings, key);
}

void gsettings_bridge_set_changed_callback(void *handle,
                                           void (*cb)(void *, const char *),
                                           void *userdata)
{
    auto *h = static_cast<BridgeHandle *>(handle);
    if (!h) return;
    h->cb = cb;
    h->userdata = userdata;
}

void gsettings_bridge_free(void *handle)
{
    auto *h = static_cast<BridgeHandle *>(handle);
    if (!h) return;
    g_signal_handlers_disconnect_by_data(h->settings, h);
    g_object_unref(h->settings);
    delete h;
}
