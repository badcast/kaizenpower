#include "GpuView.hpp"
#include "UIUtils.hpp"
#include "../core/I18n.hpp"
#include <iomanip>
#include <sstream>
#include <vector>
#include <iostream>

namespace fc {

GpuView::GpuView(std::shared_ptr<GpuManager> gpuMgr, std::shared_ptr<DBusClient> dbusClient)
    : m_gpuMgr(std::move(gpuMgr)), m_dbusClient(std::move(dbusClient)) {
    buildUI();
}

void GpuView::buildUI() {
    m_rootBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(m_rootBox, 10);
    gtk_widget_set_margin_end(m_rootBox, 10);
    gtk_widget_set_margin_top(m_rootBox, 6);
    gtk_widget_set_margin_bottom(m_rootBox, 6);

    const GpuInfo& info = m_gpuMgr->getGpuInfo();

    if (!info.present) {
        GtkWidget* status = adw_status_page_new();
        adw_status_page_set_icon_name(ADW_STATUS_PAGE(status), "show-gpu-effects-symbolic");
        adw_status_page_set_title(ADW_STATUS_PAGE(status), _("Graphics Card Not Detected"));
        adw_status_page_set_description(ADW_STATUS_PAGE(status), _("No supported graphics processor found (AMD amdgpu, NVIDIA, or Intel)."));
        gtk_box_append(GTK_BOX(m_rootBox), status);
        return;
    }

    // Header
    GtkWidget* headerBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(m_rootBox), headerBox);

    GtkWidget* gpuIcon = gtk_image_new_from_icon_name("show-gpu-effects-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(gpuIcon), 22);
    gtk_box_append(GTK_BOX(headerBox), gpuIcon);

    GtkWidget* titleBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    gtk_box_append(GTK_BOX(headerBox), titleBox);

    GtkWidget* nameLabel = gtk_label_new(info.name.c_str());
    gtk_widget_set_halign(nameLabel, GTK_ALIGN_START);
    gtk_label_set_markup(GTK_LABEL(nameLabel), ("<span font_weight='bold'>" + info.name + "</span>").c_str());
    gtk_box_append(GTK_BOX(titleBox), nameLabel);

    GtkWidget* subLabel = gtk_label_new((std::string(_("Driver: ")) + info.driver + " • " + std::string(_("Bus: ")) + (info.pciAddress.empty() ? "PCI" : info.pciAddress)).c_str());
    gtk_widget_set_halign(subLabel, GTK_ALIGN_START);
    gtk_widget_add_css_class(subLabel, "dim-label");
    gtk_box_append(GTK_BOX(titleBox), subLabel);

    // Metric Summary Cards
    GtkWidget* metricsGrid = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(m_rootBox), metricsGrid);

    GtkWidget* c1 = UIUtils::createMetricCard(_("GPU Temperature"), UIUtils::formatTemperature(info.temperatureC), &m_tempLabel);
    
    std::ostringstream pss;
    pss << std::fixed << std::setprecision(1) << info.powerW << " W";
    GtkWidget* c2 = UIUtils::createMetricCard(_("Power Consumption"), pss.str(), &m_powerLabel);
    
    GtkWidget* c3 = UIUtils::createMetricCard(_("Core Clock (SCLK)"), std::to_string(info.curSclkMhz) + " MHz", &m_sclkLabel);
    GtkWidget* c4 = UIUtils::createMetricCard(_("Memory Clock (MCLK)"), std::to_string(info.curMclkMhz) + " MHz", &m_mclkLabel);

    gtk_box_append(GTK_BOX(metricsGrid), c1);
    gtk_box_append(GTK_BOX(metricsGrid), c2);
    gtk_box_append(GTK_BOX(metricsGrid), c3);
    gtk_box_append(GTK_BOX(metricsGrid), c4);

    // Controls Section
    GtkWidget* ctrlGroup = adw_preferences_group_new();
    adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(ctrlGroup), _("GPU Frequency and Power Control"));
    gtk_box_append(GTK_BOX(m_rootBox), ctrlGroup);

    // Performance Level Dropdown Row
    AdwActionRow* perfRow = ADW_ACTION_ROW(adw_action_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(perfRow), _("DPM Performance Level"));

    std::vector<std::string> levels = info.availablePerfLevels;
    if (levels.empty()) {
        levels = {"auto", "low", "high", "manual"};
    }
    std::vector<const char*> perfPtrs;
    int selectedPerf = 0;
    for (size_t i = 0; i < levels.size(); ++i) {
        perfPtrs.push_back(levels[i].c_str());
        if (levels[i] == info.perfLevel) selectedPerf = static_cast<int>(i);
    }
    perfPtrs.push_back(nullptr);

    m_perfLevelDropDown = gtk_drop_down_new_from_strings(perfPtrs.data());
    gtk_drop_down_set_selected(GTK_DROP_DOWN(m_perfLevelDropDown), selectedPerf);
    g_signal_connect(m_perfLevelDropDown, "notify::selected", G_CALLBACK(onPerfLevelChanged), this);
    adw_action_row_add_suffix(perfRow, m_perfLevelDropDown);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(ctrlGroup), GTK_WIDGET(perfRow));

    // SCLK States Dropdown Row
    if (!info.sclkStates.empty()) {
        AdwActionRow* sclkRow = ADW_ACTION_ROW(adw_action_row_new());
        adw_preferences_row_set_title(ADW_PREFERENCES_ROW(sclkRow), _("Fix Core Clock (SCLK State)"));

        std::vector<std::string> sclkStrings;
        std::vector<const char*> sclkPtrs;
        int activeSclk = 0;
        for (size_t i = 0; i < info.sclkStates.size(); ++i) {
            std::string s = "DPM " + std::to_string(info.sclkStates[i].index) + ": " + std::to_string(info.sclkStates[i].mhz) + " MHz";
            sclkStrings.push_back(s);
            if (info.sclkStates[i].isActive) activeSclk = static_cast<int>(i);
        }
        for (const auto& s : sclkStrings) sclkPtrs.push_back(s.c_str());
        sclkPtrs.push_back(nullptr);

        m_sclkDropDown = gtk_drop_down_new_from_strings(sclkPtrs.data());
        gtk_drop_down_set_selected(GTK_DROP_DOWN(m_sclkDropDown), activeSclk);
        adw_action_row_add_suffix(sclkRow, m_sclkDropDown);
        adw_preferences_group_add(ADW_PREFERENCES_GROUP(ctrlGroup), GTK_WIDGET(sclkRow));
    }

    // MCLK States Dropdown Row
    if (!info.mclkStates.empty()) {
        AdwActionRow* mclkRow = ADW_ACTION_ROW(adw_action_row_new());
        adw_preferences_row_set_title(ADW_PREFERENCES_ROW(mclkRow), _("Fix Memory Clock (MCLK State)"));

        std::vector<std::string> mclkStrings;
        std::vector<const char*> mclkPtrs;
        int activeMclk = 0;
        for (size_t i = 0; i < info.mclkStates.size(); ++i) {
            std::string s = "DPM " + std::to_string(info.mclkStates[i].index) + ": " + std::to_string(info.mclkStates[i].mhz) + " MHz";
            mclkStrings.push_back(s);
            if (info.mclkStates[i].isActive) activeMclk = static_cast<int>(i);
        }
        for (const auto& s : mclkStrings) mclkPtrs.push_back(s.c_str());
        mclkPtrs.push_back(nullptr);

        m_mclkDropDown = gtk_drop_down_new_from_strings(mclkPtrs.data());
        gtk_drop_down_set_selected(GTK_DROP_DOWN(m_mclkDropDown), activeMclk);
        adw_action_row_add_suffix(mclkRow, m_mclkDropDown);
        adw_preferences_group_add(ADW_PREFERENCES_GROUP(ctrlGroup), GTK_WIDGET(mclkRow));
    }

    // Power Profile Row
    if (!info.powerProfiles.empty()) {
        AdwActionRow* ppRow = ADW_ACTION_ROW(adw_action_row_new());
        adw_preferences_row_set_title(ADW_PREFERENCES_ROW(ppRow), _("Power Profile"));

        std::vector<std::string> ppStrings;
        std::vector<const char*> ppPtrs;
        int activePp = 0;
        for (size_t i = 0; i < info.powerProfiles.size(); ++i) {
            ppStrings.push_back(info.powerProfiles[i].second);
            if (info.powerProfiles[i].first == info.activePowerProfile) activePp = static_cast<int>(i);
        }
        for (const auto& s : ppStrings) ppPtrs.push_back(s.c_str());
        ppPtrs.push_back(nullptr);

        m_powerProfileDropDown = gtk_drop_down_new_from_strings(ppPtrs.data());
        gtk_drop_down_set_selected(GTK_DROP_DOWN(m_powerProfileDropDown), activePp);
        adw_action_row_add_suffix(ppRow, m_powerProfileDropDown);
        adw_preferences_group_add(ADW_PREFERENCES_GROUP(ctrlGroup), GTK_WIDGET(ppRow));
    }

    // Apply Button
    GtkWidget* btnBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign(btnBox, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btnBox, 4);
    m_applyBtn = gtk_button_new_with_label(_("Apply GPU Settings"));
    gtk_widget_add_css_class(m_applyBtn, "suggested-action");
    gtk_widget_add_css_class(m_applyBtn, "pill-btn");
    g_signal_connect(m_applyBtn, "clicked", G_CALLBACK(onApplyClicked), this);
    gtk_box_append(GTK_BOX(btnBox), m_applyBtn);
    gtk_box_append(GTK_BOX(m_rootBox), btnBox);
}

void GpuView::updateData(const GpuInfo& info) {
    if (!info.present) return;

    if (m_tempLabel) {
        gtk_label_set_text(GTK_LABEL(m_tempLabel), UIUtils::formatTemperature(info.temperatureC).c_str());
    }
    if (m_powerLabel) {
        std::ostringstream pss;
        pss << std::fixed << std::setprecision(1) << info.powerW << " W";
        gtk_label_set_text(GTK_LABEL(m_powerLabel), pss.str().c_str());
    }
    if (m_sclkLabel) {
        gtk_label_set_text(GTK_LABEL(m_sclkLabel), (std::to_string(info.curSclkMhz) + " MHz").c_str());
    }
    if (m_mclkLabel) {
        gtk_label_set_text(GTK_LABEL(m_mclkLabel), (std::to_string(info.curMclkMhz) + " MHz").c_str());
    }
}

void GpuView::onPerfLevelChanged(GObject* obj, GParamSpec*, gpointer userData) {
    GpuView* self = static_cast<GpuView*>(userData);
    if (!self->m_gpuMgr) return;
    guint sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(obj));
    const auto& levels = self->m_gpuMgr->getGpuInfo().availablePerfLevels;
    if (sel < levels.size()) {
        bool isManual = (levels[sel] == "manual");
        if (self->m_sclkDropDown) gtk_widget_set_sensitive(self->m_sclkDropDown, isManual);
        if (self->m_mclkDropDown) gtk_widget_set_sensitive(self->m_mclkDropDown, isManual);
    }
}

void GpuView::onApplyClicked(GtkButton*, gpointer userData) {
    GpuView* self = static_cast<GpuView*>(userData);
    if (!self->m_gpuMgr) return;

    std::cout << "[KAIZEN GUI] Applying user GPU configuration changes" << std::endl;
    const GpuInfo& info = self->m_gpuMgr->getGpuInfo();

    if (self->m_dbusClient && self->m_dbusClient->isConnected()) {
        std::string outMsg;
        if (self->m_perfLevelDropDown) {
            guint selPerf = gtk_drop_down_get_selected(GTK_DROP_DOWN(self->m_perfLevelDropDown));
            if (selPerf < info.availablePerfLevels.size()) {
                std::string level = info.availablePerfLevels[selPerf];
                self->m_dbusClient->setGpuPerfLevel(level, outMsg);
                if (level == "manual") {
                    int sclk = -1, mclk = -1;
                    if (self->m_sclkDropDown && !info.sclkStates.empty()) {
                        guint selSclk = gtk_drop_down_get_selected(GTK_DROP_DOWN(self->m_sclkDropDown));
                        if (selSclk < info.sclkStates.size()) sclk = info.sclkStates[selSclk].index;
                    }
                    if (self->m_mclkDropDown && !info.mclkStates.empty()) {
                        guint selMclk = gtk_drop_down_get_selected(GTK_DROP_DOWN(self->m_mclkDropDown));
                        if (selMclk < info.mclkStates.size()) mclk = info.mclkStates[selMclk].index;
                    }
                    if (sclk >= 0 || mclk >= 0) {
                        self->m_dbusClient->setGpuClock(sclk, mclk, outMsg);
                    }
                }
            }
        }
        if (self->m_powerProfileDropDown && !info.powerProfiles.empty()) {
            guint selPp = gtk_drop_down_get_selected(GTK_DROP_DOWN(self->m_powerProfileDropDown));
            if (selPp < info.powerProfiles.size()) {
                self->m_dbusClient->setGpuPowerProfile(info.powerProfiles[selPp].first, outMsg);
            }
        }
        return;
    }

    // Direct fallback
    // 1. Performance Level
    if (self->m_perfLevelDropDown) {
        guint selPerf = gtk_drop_down_get_selected(GTK_DROP_DOWN(self->m_perfLevelDropDown));
        if (selPerf < info.availablePerfLevels.size()) {
            std::string level = info.availablePerfLevels[selPerf];
            self->m_gpuMgr->setPerformanceLevel(level);

            // 2. If manual, apply SCLK and MCLK states
            if (level == "manual") {
                if (self->m_sclkDropDown && !info.sclkStates.empty()) {
                    guint selSclk = gtk_drop_down_get_selected(GTK_DROP_DOWN(self->m_sclkDropDown));
                    if (selSclk < info.sclkStates.size()) {
                        self->m_gpuMgr->setSclkState(info.sclkStates[selSclk].index);
                    }
                }
                if (self->m_mclkDropDown && !info.mclkStates.empty()) {
                    guint selMclk = gtk_drop_down_get_selected(GTK_DROP_DOWN(self->m_mclkDropDown));
                    if (selMclk < info.mclkStates.size()) {
                        self->m_gpuMgr->setMclkState(info.mclkStates[selMclk].index);
                    }
                }
            }
        }
    }

    // 3. Power profile mode
    if (self->m_powerProfileDropDown && !info.powerProfiles.empty()) {
        guint selPp = gtk_drop_down_get_selected(GTK_DROP_DOWN(self->m_powerProfileDropDown));
        if (selPp < info.powerProfiles.size()) {
            self->m_gpuMgr->setPowerProfileMode(info.powerProfiles[selPp].first);
        }
    }
}

} // namespace fc
