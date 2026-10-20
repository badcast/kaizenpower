#include "CpuView.hpp"
#include "UIUtils.hpp"
#include "../core/I18n.hpp"
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace fc {

CpuView::CpuView(std::shared_ptr<CpuManager> cpuMgr, std::shared_ptr<DBusClient> dbusClient)
    : m_cpuMgr(std::move(cpuMgr)), m_dbusClient(std::move(dbusClient)) {
    buildUI();
}

void CpuView::buildUI() {
    m_rootBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(m_rootBox, 10);
    gtk_widget_set_margin_end(m_rootBox, 10);
    gtk_widget_set_margin_top(m_rootBox, 6);
    gtk_widget_set_margin_bottom(m_rootBox, 6);

    const CpuInfo& info = m_cpuMgr->getCpuInfo();

    // CPU Model Header
    GtkWidget* headerBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(m_rootBox), headerBox);

    GtkWidget* cpuIcon = gtk_image_new_from_icon_name("computer-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(cpuIcon), 20);
    gtk_box_append(GTK_BOX(headerBox), cpuIcon);

    GtkWidget* titleBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    gtk_box_append(GTK_BOX(headerBox), titleBox);

    GtkWidget* modelLabel = gtk_label_new(info.modelName.c_str());
    gtk_widget_set_halign(modelLabel, GTK_ALIGN_START);
    gtk_label_set_markup(GTK_LABEL(modelLabel), ("<span font_weight='bold'>" + info.modelName + "</span>").c_str());
    gtk_box_append(GTK_BOX(titleBox), modelLabel);

    GtkWidget* subLabel = gtk_label_new((std::string(_("Threads / Cores: ")) + std::to_string(info.coreCount) + " • " +
                                         std::string(_("Frequencies: ")) + UIUtils::formatFrequency(info.minLimitGhz) + " — " +
                                         UIUtils::formatFrequency(info.maxLimitGhz)).c_str());
    gtk_widget_set_halign(subLabel, GTK_ALIGN_START);
    gtk_widget_add_css_class(subLabel, "dim-label");
    gtk_box_append(GTK_BOX(titleBox), subLabel);

    // Metric Summary Cards
    GtkWidget* metricsGrid = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(m_rootBox), metricsGrid);

    GtkWidget* c1 = UIUtils::createMetricCard(_("Temperature"), UIUtils::formatTemperature(info.temperatureC), &m_tempLabel);
    GtkWidget* c2 = UIUtils::createMetricCard(_("Average Frequency"), UIUtils::formatFrequency(info.avgFreqGhz), &m_avgFreqLabel);
    GtkWidget* c3 = UIUtils::createMetricCard(_("Active Governor"), info.currentGovernor, &m_govStatusLabel);
    GtkWidget* c4 = UIUtils::createMetricCard(_("Energy Profile"), info.energyPreference.empty() ? "N/A" : info.energyPreference, nullptr);

    gtk_box_append(GTK_BOX(metricsGrid), c1);
    gtk_box_append(GTK_BOX(metricsGrid), c2);
    gtk_box_append(GTK_BOX(metricsGrid), c3);
    gtk_box_append(GTK_BOX(metricsGrid), c4);

    // Controls Section
    GtkWidget* ctrlGroup = adw_preferences_group_new();
    adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(ctrlGroup), _("Frequency and Power Control"));
    gtk_box_append(GTK_BOX(m_rootBox), ctrlGroup);

    // Governor Dropdown Row
    AdwActionRow* govRow = ADW_ACTION_ROW(adw_action_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(govRow), _("Scaling Governor"));

    std::vector<std::string> govs = info.availableGovernors;
    if (govs.empty()) {
        govs = {"schedutil", "powersave", "performance"};
    }
    std::vector<const char*> govPtrs;
    int selectedGov = 0;
    for (size_t i = 0; i < govs.size(); ++i) {
        govPtrs.push_back(govs[i].c_str());
        if (govs[i] == info.currentGovernor) selectedGov = static_cast<int>(i);
    }
    govPtrs.push_back(nullptr);

    m_govDropDown = gtk_drop_down_new_from_strings(govPtrs.data());
    gtk_drop_down_set_selected(GTK_DROP_DOWN(m_govDropDown), selectedGov);
    adw_action_row_add_suffix(govRow, m_govDropDown);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(ctrlGroup), GTK_WIDGET(govRow));

    // Energy Performance Preference Row
    if (!info.availableEnergyPreferences.empty()) {
        AdwActionRow* eppRow = ADW_ACTION_ROW(adw_action_row_new());
        adw_preferences_row_set_title(ADW_PREFERENCES_ROW(eppRow), _("Energy Preference (EPP)"));

        std::vector<const char*> eppPtrs;
        int selectedEpp = 0;
        for (size_t i = 0; i < info.availableEnergyPreferences.size(); ++i) {
            eppPtrs.push_back(info.availableEnergyPreferences[i].c_str());
            if (info.availableEnergyPreferences[i] == info.energyPreference) selectedEpp = static_cast<int>(i);
        }
        eppPtrs.push_back(nullptr);

        m_eppDropDown = gtk_drop_down_new_from_strings(eppPtrs.data());
        gtk_drop_down_set_selected(GTK_DROP_DOWN(m_eppDropDown), selectedEpp);
        adw_action_row_add_suffix(eppRow, m_eppDropDown);
        adw_preferences_group_add(ADW_PREFERENCES_GROUP(ctrlGroup), GTK_WIDGET(eppRow));
    }

    // Turbo Boost Row
    if (info.hasTurboControl) {
        AdwActionRow* turboRow = ADW_ACTION_ROW(adw_action_row_new());
        adw_preferences_row_set_title(ADW_PREFERENCES_ROW(turboRow), _("Intel Turbo Boost / AMD Core Boost"));

        m_turboSwitch = gtk_switch_new();
        gtk_switch_set_active(GTK_SWITCH(m_turboSwitch), info.turboBoost);
        adw_action_row_add_suffix(turboRow, m_turboSwitch);
        adw_preferences_group_add(ADW_PREFERENCES_GROUP(ctrlGroup), GTK_WIDGET(turboRow));
    }

    // Min Freq Limit Row
    double minLim = (info.minLimitGhz > 0.0) ? info.minLimitGhz : 0.8;
    double maxLim = (info.maxLimitGhz > minLim) ? info.maxLimitGhz : (minLim + 2.0);

    AdwActionRow* minFreqRow = ADW_ACTION_ROW(adw_action_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(minFreqRow), _("Minimum CPU Frequency"));
    m_minFreqScale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, minLim, maxLim, 0.1);
    gtk_widget_set_size_request(m_minFreqScale, 160, -1);
    gtk_range_set_value(GTK_RANGE(m_minFreqScale), minLim);
    g_signal_connect(m_minFreqScale, "value-changed", G_CALLBACK(onMinScaleChanged), this);
    m_minFreqValLabel = gtk_label_new(UIUtils::formatFrequency(minLim).c_str());
    gtk_widget_set_size_request(m_minFreqValLabel, 60, -1);
    adw_action_row_add_suffix(minFreqRow, m_minFreqScale);
    adw_action_row_add_suffix(minFreqRow, m_minFreqValLabel);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(ctrlGroup), GTK_WIDGET(minFreqRow));

    // Max Freq Limit Row
    AdwActionRow* maxFreqRow = ADW_ACTION_ROW(adw_action_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(maxFreqRow), _("Maximum CPU Frequency"));
    m_maxFreqScale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, minLim, maxLim, 0.1);
    gtk_widget_set_size_request(m_maxFreqScale, 160, -1);
    gtk_range_set_value(GTK_RANGE(m_maxFreqScale), maxLim);
    g_signal_connect(m_maxFreqScale, "value-changed", G_CALLBACK(onMaxScaleChanged), this);
    m_maxFreqValLabel = gtk_label_new(UIUtils::formatFrequency(maxLim).c_str());
    gtk_widget_set_size_request(m_maxFreqValLabel, 60, -1);
    adw_action_row_add_suffix(maxFreqRow, m_maxFreqScale);
    adw_action_row_add_suffix(maxFreqRow, m_maxFreqValLabel);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(ctrlGroup), GTK_WIDGET(maxFreqRow));

    // Apply Button Row
    GtkWidget* btnBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign(btnBox, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btnBox, 4);
    m_applyBtn = gtk_button_new_with_label(_("Apply CPU Settings"));
    gtk_widget_add_css_class(m_applyBtn, "suggested-action");
    gtk_widget_add_css_class(m_applyBtn, "pill-btn");
    g_signal_connect(m_applyBtn, "clicked", G_CALLBACK(onApplyClicked), this);
    gtk_box_append(GTK_BOX(btnBox), m_applyBtn);
    gtk_box_append(GTK_BOX(m_rootBox), btnBox);

    // Compact Per-core frequencies overview (2-column grid instead of separate tall rows)
    GtkWidget* coresBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(coresBox, "fan-card");
    gtk_box_append(GTK_BOX(m_rootBox), coresBox);

    GtkWidget* coresHdr = gtk_label_new(_("Per-Core Frequency Monitoring"));
    gtk_widget_set_halign(coresHdr, GTK_ALIGN_START);
    gtk_widget_add_css_class(coresHdr, "metric-title");
    gtk_box_append(GTK_BOX(coresBox), coresHdr);

    GtkWidget* coresGrid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(coresGrid), 4);
    gtk_grid_set_column_spacing(GTK_GRID(coresGrid), 12);
    gtk_grid_set_row_homogeneous(GTK_GRID(coresGrid), TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(coresGrid), TRUE);
    gtk_box_append(GTK_BOX(coresBox), coresGrid);

    m_coreWidgets.clear();
    for (size_t i = 0; i < info.cores.size(); ++i) {
        const auto& core = info.cores[i];
        GtkWidget* itemBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

        GtkWidget* nameLbl = gtk_label_new((std::string(_("Core ")) + std::to_string(core.coreId)).c_str());
        gtk_widget_set_size_request(nameLbl, 50, -1);
        gtk_widget_set_halign(nameLbl, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(itemBox), nameLbl);

        GtkWidget* pbar = gtk_progress_bar_new();
        gtk_widget_set_hexpand(pbar, TRUE);
        gtk_widget_set_valign(pbar, GTK_ALIGN_CENTER);
        double frac = (info.maxLimitGhz > 0) ? (core.curFreqGhz / info.maxLimitGhz) : 0.0;
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pbar), std::clamp(frac, 0.0, 1.0));
        gtk_box_append(GTK_BOX(itemBox), pbar);

        GtkWidget* lbl = gtk_label_new(UIUtils::formatFrequency(core.curFreqGhz).c_str());
        gtk_widget_set_size_request(lbl, 60, -1);
        gtk_widget_set_halign(lbl, GTK_ALIGN_END);
        gtk_box_append(GTK_BOX(itemBox), lbl);

        int col = static_cast<int>(i % 2);
        int row = static_cast<int>(i / 2);
        gtk_grid_attach(GTK_GRID(coresGrid), itemBox, col, row, 1, 1);
        m_coreWidgets.push_back({lbl, pbar});
    }
}

void CpuView::updateData(const CpuInfo& info) {
    if (m_tempLabel) {
        gtk_label_set_text(GTK_LABEL(m_tempLabel), UIUtils::formatTemperature(info.temperatureC).c_str());
    }
    if (m_avgFreqLabel) {
        gtk_label_set_text(GTK_LABEL(m_avgFreqLabel), UIUtils::formatFrequency(info.avgFreqGhz).c_str());
    }
    if (m_govStatusLabel) {
        gtk_label_set_text(GTK_LABEL(m_govStatusLabel), info.currentGovernor.c_str());
    }

    // Update per-core meters
    for (size_t i = 0; i < info.cores.size() && i < m_coreWidgets.size(); ++i) {
        gtk_label_set_text(GTK_LABEL(m_coreWidgets[i].label), UIUtils::formatFrequency(info.cores[i].curFreqGhz).c_str());
        double frac = (info.maxLimitGhz > 0) ? (info.cores[i].curFreqGhz / info.maxLimitGhz) : 0.0;
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(m_coreWidgets[i].progress), std::clamp(frac, 0.0, 1.0));
    }
}

void CpuView::onMinScaleChanged(GtkRange* range, gpointer userData) {
    CpuView* self = static_cast<CpuView*>(userData);
    double val = gtk_range_get_value(range);
    if (self->m_minFreqValLabel) {
        gtk_label_set_text(GTK_LABEL(self->m_minFreqValLabel), UIUtils::formatFrequency(val).c_str());
    }
}

void CpuView::onMaxScaleChanged(GtkRange* range, gpointer userData) {
    CpuView* self = static_cast<CpuView*>(userData);
    double val = gtk_range_get_value(range);
    if (self->m_maxFreqValLabel) {
        gtk_label_set_text(GTK_LABEL(self->m_maxFreqValLabel), UIUtils::formatFrequency(val).c_str());
    }
}

void CpuView::onApplyClicked(GtkButton*, gpointer userData) {
    CpuView* self = static_cast<CpuView*>(userData);
    if (!self->m_cpuMgr) return;

    std::cout << "[KAIZEN GUI] Applying user CPU configuration changes" << std::endl;
    const CpuInfo& info = self->m_cpuMgr->getCpuInfo();

    if (self->m_dbusClient && self->m_dbusClient->isConnected()) {
        std::string outMsg;
        if (self->m_govDropDown) {
            guint selGov = gtk_drop_down_get_selected(GTK_DROP_DOWN(self->m_govDropDown));
            if (selGov < info.availableGovernors.size()) {
                self->m_dbusClient->setCpuGovernor(info.availableGovernors[selGov], outMsg);
            }
        }
        if (self->m_eppDropDown && !info.availableEnergyPreferences.empty()) {
            guint selEpp = gtk_drop_down_get_selected(GTK_DROP_DOWN(self->m_eppDropDown));
            if (selEpp < info.availableEnergyPreferences.size()) {
                self->m_dbusClient->setCpuEnergyPreference(info.availableEnergyPreferences[selEpp], outMsg);
            }
        }
        if (self->m_turboSwitch) {
            gboolean turboActive = gtk_switch_get_active(GTK_SWITCH(self->m_turboSwitch));
            self->m_dbusClient->setCpuTurbo(turboActive, outMsg);
        }
        if (self->m_minFreqScale && self->m_maxFreqScale) {
            double minG = gtk_range_get_value(GTK_RANGE(self->m_minFreqScale));
            double maxG = gtk_range_get_value(GTK_RANGE(self->m_maxFreqScale));
            if (minG <= maxG) {
                self->m_dbusClient->setCpuLimits(minG, maxG, outMsg);
            }
        }
        return;
    }

    // Direct fallback
    // 1. Governor
    if (self->m_govDropDown) {
        guint selGov = gtk_drop_down_get_selected(GTK_DROP_DOWN(self->m_govDropDown));
        if (selGov < info.availableGovernors.size()) {
            self->m_cpuMgr->setGovernor(info.availableGovernors[selGov]);
        }
    }

    // 2. EPP
    if (self->m_eppDropDown && !info.availableEnergyPreferences.empty()) {
        guint selEpp = gtk_drop_down_get_selected(GTK_DROP_DOWN(self->m_eppDropDown));
        if (selEpp < info.availableEnergyPreferences.size()) {
            self->m_cpuMgr->setEnergyPreference(info.availableEnergyPreferences[selEpp]);
        }
    }

    // 3. Turbo Boost
    if (self->m_turboSwitch) {
        gboolean turboActive = gtk_switch_get_active(GTK_SWITCH(self->m_turboSwitch));
        self->m_cpuMgr->setTurboBoost(turboActive);
    }

    // 4. Frequency Limits
    if (self->m_minFreqScale && self->m_maxFreqScale) {
        double minG = gtk_range_get_value(GTK_RANGE(self->m_minFreqScale));
        double maxG = gtk_range_get_value(GTK_RANGE(self->m_maxFreqScale));
        if (minG <= maxG) {
            self->m_cpuMgr->setFreqLimits(minG, maxG);
        }
    }
}

} // namespace fc
