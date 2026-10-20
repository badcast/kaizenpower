#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../core/Types.hpp"
#include "../core/GpuManager.hpp"
#include "../dbus/DBusClient.hpp"
#include <memory>

namespace fc {

class GpuView {
public:
    GpuView(std::shared_ptr<GpuManager> gpuMgr, std::shared_ptr<DBusClient> dbusClient = nullptr);
    ~GpuView() = default;

    GtkWidget* getWidget() const { return m_rootBox; }
    void updateData(const GpuInfo& info);

private:
    std::shared_ptr<GpuManager> m_gpuMgr;
    std::shared_ptr<DBusClient> m_dbusClient;

    GtkWidget* m_rootBox = nullptr;

    // Metrics labels
    GtkWidget* m_tempLabel = nullptr;
    GtkWidget* m_powerLabel = nullptr;
    GtkWidget* m_sclkLabel = nullptr;
    GtkWidget* m_mclkLabel = nullptr;

    // Controls
    GtkWidget* m_perfLevelDropDown = nullptr;
    GtkWidget* m_sclkDropDown = nullptr;
    GtkWidget* m_mclkDropDown = nullptr;
    GtkWidget* m_powerProfileDropDown = nullptr;
    GtkWidget* m_applyBtn = nullptr;

    void buildUI();
    static void onApplyClicked(GtkButton* btn, gpointer userData);
    static void onPerfLevelChanged(GObject* obj, GParamSpec* pspec, gpointer userData);
};

} // namespace fc
