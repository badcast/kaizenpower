#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../core/Types.hpp"
#include "../core/FanManager.hpp"
#include "../dbus/DBusClient.hpp"
#include "FanCurveWidget.hpp"
#include <memory>

namespace fc {

class FanCard {
public:
    FanCard(const FanInfo& info, std::shared_ptr<FanManager> fanMgr, std::shared_ptr<DBusClient> dbusClient = nullptr);
    ~FanCard() = default;

    GtkWidget* getWidget() const { return m_rootBox; }
    std::string getFanId() const { return m_info.id; }
    void updateData(const FanInfo& info, double currentTemp = 0.0);

private:
    FanInfo m_info;
    std::shared_ptr<FanManager> m_fanMgr;
    std::shared_ptr<DBusClient> m_dbusClient;

    GtkWidget* m_rootBox = nullptr;
    GtkWidget* m_icon = nullptr;
    GtkWidget* m_nameLabel = nullptr;
    GtkWidget* m_badgeLabel = nullptr;
    GtkWidget* m_reasonLabel = nullptr;
    GtkWidget* m_typeDropDown = nullptr;

    GtkWidget* m_rpmValueLabel = nullptr;
    GtkWidget* m_rpmProgress = nullptr;

    // Mode Selector
    GtkWidget* m_modeDropDown = nullptr;

    // Simple Mode Container
    GtkWidget* m_simpleBox = nullptr;
    GtkWidget* m_speedScale = nullptr;
    GtkWidget* m_speedPercentLabel = nullptr;
    GtkWidget* m_applyBtn = nullptr;

    // Curve Mode Container
    GtkWidget* m_curveBox = nullptr;
    std::unique_ptr<FanCurveWidget> m_curveWidget;
    GtkWidget* m_curveInfoLabel = nullptr;

    // Presets buttons tracking
    std::vector<std::pair<int, GtkWidget*>> m_speedPresetBtns;
    std::vector<std::pair<int, GtkWidget*>> m_curvePresetBtns;

    bool m_updatingFromModel = false;

    void buildUI();
    void updateBadge(FanType type);
    void updateModeVisibility(FanControlMode mode);
    void updateSpeedPresetHighlight(int currentPercent);
    void updateCurvePresetHighlight(int presetType);

    static void onModeChanged(GObject* obj, GParamSpec* pspec, gpointer userData);
    static void onScaleValueChanged(GtkRange* range, gpointer userData);
    static void onPresetClicked(GtkButton* btn, gpointer userData);
    static void onApplyClicked(GtkButton* btn, gpointer userData);
    static void onTypeDropDownChanged(GObject* obj, GParamSpec* pspec, gpointer userData);
    static void onCurvePresetClicked(GtkButton* btn, gpointer userData);
};

} // namespace fc
