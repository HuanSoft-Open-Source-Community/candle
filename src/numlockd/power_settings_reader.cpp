#include "power_settings_reader.h"

#include <DConfig>
#ifdef HAVE_GIO
#include <gio/gio.h>
#endif

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>

DCORE_USE_NAMESPACE

// ============================================================================
// 常量定义
// ============================================================================

// ---- DConfig（deepin 23/25 / UOS 25） ----
static const char kDConfigAppId[]  = "org.deepin.dde.daemon.power";
static const char kDConfigKeyAc[]  = "linePowerScreenBlackDelay";
static const char kDConfigKeyBat[] = "batteryScreenBlackDelay";

// ---- GSettings（UOS 20 兜底） ----
static const char kGSettingsSchema[] = "com.deepin.dde.power";
static const char kGSettingsKeyAc[]  = "screenBlackDelayOnAc";
static const char kGSettingsKeyBat[] = "screenBlackDelayOnBattery";

// ---- D-Bus 供电状态 ----
static const char kPowerService[]   = "org.deepin.dde.Power1";
static const char kPowerPath[]      = "/org/deepin/dde/Power1";
static const char kPowerInterface[] = "org.deepin.dde.Power1";


PowerSettingsReader::PowerSettingsReader(QObject *parent)
    : QObject(parent)
{
}

PowerSettingsReader::~PowerSettingsReader()
{
#ifdef HAVE_GIO
    if (m_gsettings) {
        g_object_unref(m_gsettings);
        m_gsettings = nullptr;
    }
#endif
    if (m_dconfig) {
        delete m_dconfig;
        m_dconfig = nullptr;
    }
}

bool PowerSettingsReader::init()
{
    // 优先 DConfig
    if (tryDConfig()) {
        m_available = true;
        refreshDelay();
        qDebug() << "PowerSettingsReader: using DConfig, delay:" << m_delaySeconds << "s";
        return true;
    }

    // 兜底 GSettings
#ifdef HAVE_GIO
    if (tryGSettings()) {
        m_available = true;
        refreshDelay();
        qDebug() << "PowerSettingsReader: using GSettings, delay:" << m_delaySeconds << "s";
        return true;
    }
#endif

    qWarning() << "PowerSettingsReader: no power settings backend available";
    m_available = false;
    return false;
}

// -------- DConfig --------

bool PowerSettingsReader::tryDConfig()
{
    m_dconfig = DConfig::create(kDConfigAppId, QString(), this);
    if (!m_dconfig || !m_dconfig->isValid()) {
        qDebug() << "PowerSettingsReader: DConfig not available for" << kDConfigAppId;
        delete m_dconfig;
        m_dconfig = nullptr;
        return false;
    }

    // 检查关键 key 是否存在
    if (!m_dconfig->keyList().contains(kDConfigKeyAc) &&
        !m_dconfig->keyList().contains(kDConfigKeyBat)) {
        qDebug() << "PowerSettingsReader: DConfig power keys not found";
        delete m_dconfig;
        m_dconfig = nullptr;
        return false;
    }

    connect(m_dconfig, &DConfig::valueChanged,
            this, &PowerSettingsReader::onDConfigValueChanged);
    return true;
}

int PowerSettingsReader::readDelayFromDConfig() const
{
    if (!m_dconfig) return -1;
    const char *key = isOnBattery() ? kDConfigKeyBat : kDConfigKeyAc;
    return m_dconfig->value(key, -1).toInt();
}

void PowerSettingsReader::onDConfigValueChanged(const QString &key)
{
    if (key == QString::fromLatin1(kDConfigKeyAc) ||
        key == QString::fromLatin1(kDConfigKeyBat)) {
        refreshDelay();
    }
}

// -------- GSettings（UOS 20 兜底） --------

#ifdef HAVE_GIO
// GSettings changed 回调（C 函数，无捕获）
static void onGSettingsChangedCb(GSettings *, const char *key, gpointer userData)
{
    auto *self = static_cast<PowerSettingsReader *>(userData);
    self->onGSettingsChanged(QString::fromLatin1(key));
}
#endif

#ifdef HAVE_GIO

bool PowerSettingsReader::tryGSettings()
{
    GSettings *s = g_settings_new(kGSettingsSchema);
    if (!s) {
        qDebug() << "PowerSettingsReader: GSettings schema not found:" << kGSettingsSchema;
        return false;
    }

    // 检查 key 存在（g_settings_get_int 对不存在的 key 返回 0，无法区分）
    // 尝试读取一次看是否返回合理值
    int acDelay = g_settings_get_int(s, kGSettingsKeyAc);
    int batDelay = g_settings_get_int(s, kGSettingsKeyBat);
    if (acDelay <= 0 && batDelay <= 0) {
        qDebug() << "PowerSettingsReader: GSettings power keys return zero or missing";
        g_object_unref(s);
        return false;
    }

    m_gsettings = s;
    g_signal_connect(s, "changed", G_CALLBACK(onGSettingsChangedCb), this);

    return true;
}

int PowerSettingsReader::readDelayFromGSettings() const
{
    if (!m_gsettings) return -1;
    const char *key = isOnBattery() ? kGSettingsKeyBat : kGSettingsKeyAc;
    return g_settings_get_int(m_gsettings, key);
}

void PowerSettingsReader::onGSettingsChanged(const QString &key)
{
    if (key == QString::fromLatin1(kGSettingsKeyAc) ||
        key == QString::fromLatin1(kGSettingsKeyBat)) {
        refreshDelay();
    }
}

#endif // HAVE_GIO

// -------- 公共刷新逻辑 --------

void PowerSettingsReader::refreshDelay()
{
    int newDelay = -1;
    if (m_dconfig) {
        newDelay = readDelayFromDConfig();
    }
#ifdef HAVE_GIO
    else if (m_gsettings) {
        newDelay = readDelayFromGSettings();
    }
#endif
    if (newDelay != m_delaySeconds) {
        m_delaySeconds = newDelay;
        m_available = (newDelay > 0);
        emit screenBlackDelayChanged(newDelay);
    }
}

// -------- 供电场景 --------

bool PowerSettingsReader::isOnBattery() const
{
    QDBusInterface iface(QString::fromLatin1(kPowerService),
                         QString::fromLatin1(kPowerPath),
                         QString::fromLatin1(kPowerInterface),
                         QDBusConnection::systemBus());
    if (!iface.isValid()) return false;

    QDBusReply<QVariant> reply = iface.call(QStringLiteral("Get"),
                                           kPowerInterface,
                                           QStringLiteral("OnBattery"));
    if (!reply.isValid()) return false;

    // OnBattery 可能是 bool 或包含 bool 的 QVariant
    bool ok = false;
    bool onBattery = reply.value().toBool();
    Q_UNUSED(ok);
    return onBattery;
}
