#pragma once

#include "../dbus/DBusClient.hpp"
#include "../core/CpuManager.hpp"
#include "../core/GpuManager.hpp"
#include "../core/FanManager.hpp"
#include "../core/ProfileManager.hpp"
#include <memory>
#include <string>

namespace fc {

class TuiApp {
public:
    TuiApp();
    ~TuiApp();

    int run();

private:
    std::shared_ptr<DBusClient> m_dbusClient;
    std::shared_ptr<CpuManager> m_cpuMgr;
    std::shared_ptr<GpuManager> m_gpuMgr;
    std::shared_ptr<FanManager> m_fanMgr;
    std::shared_ptr<ProfileManager> m_profileMgr;

    bool m_useDirect = false;
    std::string m_statusMessage = "Ready";

    void initColors();
    void renderDashboard();
    void drawProgressBar(int y, int x, int width, double fraction, int colorPair);
};

} // namespace fc
