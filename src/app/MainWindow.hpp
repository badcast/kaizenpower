#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../core/FanManager.hpp"
#include "../core/CpuManager.hpp"
#include "../core/GpuManager.hpp"
#include "../core/ProfileManager.hpp"
#include "../ui/FanCard.hpp"
#include "../ui/CpuView.hpp"
#include "../ui/GpuView.hpp"
#include "../ui/ProfilesView.hpp"
#include "../ui/SummaryView.hpp"
#include "../dbus/DBusClient.hpp"
#include <memory>
#include <vector>

namespace fc {

class MainWindow {
public:
    MainWindow(AdwApplication* app, const std::string& defaultTab = "summary");
    ~MainWindow();

    void show();

private:
    AdwApplication* m_app = nullptr;
    AdwApplicationWindow* m_window = nullptr;
    AdwToastOverlay* m_toastOverlay = nullptr;
    AdwBanner* m_privilegeBanner = nullptr;

    // Managers
    std::shared_ptr<FanManager> m_fanMgr;
    std::shared_ptr<CpuManager> m_cpuMgr;
    std::shared_ptr<GpuManager> m_gpuMgr;
    std::shared_ptr<ProfileManager> m_profileMgr;
    std::shared_ptr<DBusClient> m_dbusClient;

    // UI Views
    std::unique_ptr<SummaryView> m_summaryView;
    GtkWidget* m_fansContainer = nullptr;
    std::vector<std::unique_ptr<FanCard>> m_fanCards;
    std::unique_ptr<CpuView> m_cpuView;
    std::unique_ptr<GpuView> m_gpuView;
    std::unique_ptr<ProfilesView> m_profilesView;

    // Timer for live updates
    guint m_timerId = 0;
    bool m_livePolling = true;

    // Status widgets
    GtkWidget* m_fanSummaryLabel = nullptr;
    GtkWidget* m_langDropDown = nullptr;
    GtkWidget* m_privBadge = nullptr;
    GtkWidget* m_elevateBtn = nullptr;
    GtkWidget* m_aboutBtn = nullptr;

    void buildUI();
    void refreshData();
    void showToast(const std::string& message);
    void showAboutDialog();

    static gboolean onTimerTick(gpointer userData);
    static void onRefreshClicked(GtkButton* btn, gpointer userData);
    static void onAboutClicked(GtkButton* btn, gpointer userData);
    static void onElevateClicked(GtkButton* btn, gpointer userData);
    static void onBannerButtonClicked(AdwBanner* banner, gpointer userData);
    static void onLanguageChanged(GObject* obj, GParamSpec* pspec, gpointer userData);
};

} // namespace fc
