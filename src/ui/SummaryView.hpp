#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../core/Types.hpp"
#include "../core/CpuManager.hpp"
#include "../core/GpuManager.hpp"
#include "../core/FanManager.hpp"
#include "../core/ProfileManager.hpp"
#include "../dbus/DBusClient.hpp"
#include <memory>
#include <functional>

namespace fc {

class SummaryView {
public:
    SummaryView(std::shared_ptr<CpuManager> cpuMgr,
                std::shared_ptr<GpuManager> gpuMgr,
                std::shared_ptr<FanManager> fanMgr,
                std::shared_ptr<ProfileManager> profileMgr,
                std::shared_ptr<DBusClient> dbusClient);
    ~SummaryView() = default;

    GtkWidget* getWidget() const { return m_rootBox; }

    void updateData(const CpuInfo& cpu,
                    const GpuInfo& gpu,
                    const std::vector<FanInfo>& fans,
                    const std::string& activeProfile);
    void updateActiveProfile(SystemProfile profile, bool isAuto);

    void setOnAutoAllCallback(std::function<void()> cb) { m_onAutoAll = std::move(cb); }
    void setOnProfileCallback(std::function<void(SystemProfile)> cb) { m_onProfile = std::move(cb); }
    void setOnAboutCallback(std::function<void()> cb) { m_onAbout = std::move(cb); }

private:
    std::shared_ptr<CpuManager> m_cpuMgr;
    std::shared_ptr<GpuManager> m_gpuMgr;
    std::shared_ptr<FanManager> m_fanMgr;
    std::shared_ptr<ProfileManager> m_profileMgr;
    std::shared_ptr<DBusClient> m_dbusClient;

    GtkWidget* m_rootBox = nullptr;

    // Quick profile action buttons
    GtkWidget* m_autoBtn = nullptr;
    std::vector<std::pair<SystemProfile, GtkWidget*>> m_profileBtns;

    // OS Header Widgets
    GtkWidget* m_osLogoImage = nullptr;
    GtkWidget* m_osNameLabel = nullptr;
    GtkWidget* m_kernelBadge = nullptr;
    GtkWidget* m_archBadge = nullptr;
    GtkWidget* m_uptimeBadge = nullptr;
    GtkWidget* m_daemonStatusBadge = nullptr;
    GtkWidget* m_appLogoBtn = nullptr;

    // CPU Card Widgets
    GtkWidget* m_cpuModelLabel = nullptr;
    GtkWidget* m_cpuTempBadge = nullptr;
    GtkWidget* m_cpuSubLabel = nullptr;

    // GPU Card Widgets
    GtkWidget* m_gpuModelLabel = nullptr;
    GtkWidget* m_gpuTempBadge = nullptr;
    GtkWidget* m_gpuSubLabel = nullptr;

    // Fans Card Widgets
    GtkWidget* m_fanSummaryLabel = nullptr;
    GtkWidget* m_fanSpeedBadge = nullptr;
    GtkWidget* m_fanSubLabel = nullptr;

    // Callbacks
    std::function<void()> m_onAutoAll;
    std::function<void(SystemProfile)> m_onProfile;
    std::function<void()> m_onAbout;

    void buildUI();
    static void onAutoClicked(GtkButton* btn, gpointer userData);
    static void onProfileClicked(GtkButton* btn, gpointer userData);
    static void onAboutClicked(GtkButton* btn, gpointer userData);
};

} // namespace fc
