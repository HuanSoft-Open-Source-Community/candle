#ifndef POWER_SETTINGS_READER_H
#define POWER_SETTINGS_READER_H

#include <QObject>

class DConfig;

// GSettings（libgio，UOS 20 兜底），编译时可能不可用
#ifdef HAVE_GIO
typedef struct _GSettings GSettings;
#endif

/**
 * @brief 跨平台电源设置读取器
 *
 * 读取系统"自动熄屏"延时（秒），适配不同平台：
 * - deepin 23/25 + UOS 25：DConfig（org.deepin.dde.daemon.power）
 * - UOS 20：QGSettings（com.deepin.dde.power）
 *
 * 通过 D-Bus org.deepin.dde.Power1.OnBattery 判断供电场景，
 * 自动在「交流电」与「电池」延时之间切换。
 */
class PowerSettingsReader : public QObject
{
    Q_OBJECT

public:
    explicit PowerSettingsReader(QObject *parent = nullptr);
    ~PowerSettingsReader() override;

    /**
     * @brief 初始化并读取当前熄屏延时
     * @return 是否成功（至少一种读取方式可用）
     */
    bool init();

    /**
     * @brief 当前熄屏延时（秒），-1 表示不可读
     */
    int screenBlackDelay() const { return m_delaySeconds; }

    /**
     * @brief 是否可用（成功读取过）
     */
    bool isAvailable() const { return m_available; }

    // 供 GSettings C 回调使用
    void onGSettingsChanged(const QString &key);

signals:
    /**
     * @brief 熄屏延时发生变化时发出
     * @param seconds 新的熄屏延时（秒），-1 表示变为不可读
     */
    void screenBlackDelayChanged(int seconds);

private slots:
    void onDConfigValueChanged(const QString &key);
    void refreshDelay();

private:
    bool tryDConfig();
    bool tryGSettings();
    int readDelayFromDConfig() const;
    int readDelayFromGSettings() const;
    bool isOnBattery() const;

    DConfig *m_dconfig = nullptr;
#ifdef HAVE_GIO
    GSettings *m_gsettings = nullptr;
#endif
    int m_delaySeconds = -1;
    bool        m_available = false;
};

#endif // POWER_SETTINGS_READER_H
