#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../core/Types.hpp"
#include "../core/CpuManager.hpp"
#include "../dbus/DBusClient.hpp"
#include <memory>
#include <vector>

namespace fc {

class CpuView {
public:
    CpuView(std::shared_ptr<CpuManager> cpuMgr, std::shared_ptr<DBusClient> dbusClient = nullptr);
    ~CpuView() = default;

    GtkWidget* getWidget() const { return m_rootBox; }
    void updateData(const CpuInfo& info);

private:
    std::shared_ptr<CpuManager> m_cpuMgr;
    std::shared_ptr<DBusClient> m_dbusClient;

    GtkWidget* m_rootBox = nullptr;

    // Metrics labels
    GtkWidget* m_tempLabel = nullptr;
    GtkWidget* m_avgFreqLabel = nullptr;
    GtkWidget* m_coresCountLabel = nullptr;
    GtkWidget* m_govStatusLabel = nullptr;

    // Controls
    GtkWidget* m_govDropDown = nullptr;
    GtkWidget* m_eppDropDown = nullptr;
    GtkWidget* m_turboSwitch = nullptr;
    GtkWidget* m_minFreqScale = nullptr;
    GtkWidget* m_maxFreqScale = nullptr;
    GtkWidget* m_minFreqValLabel = nullptr;
    GtkWidget* m_maxFreqValLabel = nullptr;
    GtkWidget* m_applyBtn = nullptr;

    // Per-core widgets
    struct CoreWidget {
        GtkWidget* label;
        GtkWidget* progress;
    };
    std::vector<CoreWidget> m_coreWidgets;

    bool m_updatingFromModel = false;

    void buildUI();
    static void onApplyClicked(GtkButton* btn, gpointer userData);
    static void onMinScaleChanged(GtkRange* range, gpointer userData);
    static void onMaxScaleChanged(GtkRange* range, gpointer userData);
};

} // namespace fc
