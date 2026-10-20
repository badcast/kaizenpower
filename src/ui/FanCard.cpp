#include "FanCard.hpp"
#include "UIUtils.hpp"
#include "../core/I18n.hpp"
#include "../core/JsonUtils.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace fc {

FanCard::FanCard(const FanInfo& info, std::shared_ptr<FanManager> fanMgr, std::shared_ptr<DBusClient> dbusClient)
    : m_info(info), m_fanMgr(std::move(fanMgr)), m_dbusClient(std::move(dbusClient)) {
    buildUI();
}

void FanCard::updateBadge(FanType type) {
    if (!m_badgeLabel) return;
    gtk_label_set_text(GTK_LABEL(m_badgeLabel), fanTypeBadgeText(type).c_str());

    gtk_widget_remove_css_class(m_badgeLabel, "fan-badge-cpu");
    gtk_widget_remove_css_class(m_badgeLabel, "fan-badge-gpu");
    gtk_widget_remove_css_class(m_badgeLabel, "fan-badge-case");
    gtk_widget_remove_css_class(m_badgeLabel, "fan-badge-unknown");
    gtk_widget_add_css_class(m_badgeLabel, fanTypeCssClass(type).c_str());

    if (m_icon) {
        gtk_image_set_from_icon_name(GTK_IMAGE(m_icon), fanTypeIcon(type).c_str());
    }
}

void FanCard::updateModeVisibility(FanControlMode mode) {
    if (m_simpleBox) {
        gtk_widget_set_visible(m_simpleBox, (mode == FanControlMode::Simple));
    }
    if (m_curveBox) {
        gtk_widget_set_visible(m_curveBox, (mode == FanControlMode::Curve));
    }
}

void FanCard::buildUI() {
    m_rootBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(m_rootBox, "fan-card");

    // Unified Compact Header Row
    GtkWidget* row1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(m_rootBox), row1);

    m_icon = gtk_image_new_from_icon_name(fanTypeIcon(m_info.userType).c_str());
    gtk_image_set_pixel_size(GTK_IMAGE(m_icon), 16);
    gtk_box_append(GTK_BOX(row1), m_icon);

    std::string title = m_info.label.empty() ? (std::string(_("Fan ")) + std::to_string(m_info.fanIndex) + " (" + m_info.hwmonName + ")")
                                            : m_info.label;
    m_nameLabel = gtk_label_new(title.c_str());
    gtk_widget_set_halign(m_nameLabel, GTK_ALIGN_START);
    gtk_label_set_markup(GTK_LABEL(m_nameLabel), ("<span font_weight='bold'>" + title + "</span>").c_str());
    std::string reasonText = m_info.detectionReason.empty() ? _("Detected automatically") : m_info.detectionReason;
    gtk_widget_set_tooltip_text(m_nameLabel, reasonText.c_str());
    gtk_box_append(GTK_BOX(row1), m_nameLabel);

    // Badge
    m_badgeLabel = UIUtils::createBadge(fanTypeBadgeText(m_info.userType), fanTypeCssClass(m_info.userType));
    gtk_box_append(GTK_BOX(row1), m_badgeLabel);

    // RPM Value
    m_rpmValueLabel = gtk_label_new(UIUtils::formatRpm(m_info.rpm).c_str());
    gtk_widget_add_css_class(m_rpmValueLabel, "rpm-value");
    gtk_widget_set_size_request(m_rpmValueLabel, 75, -1);
    gtk_widget_set_halign(m_rpmValueLabel, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(row1), m_rpmValueLabel);

    // Compact progress bar for RPM
    m_rpmProgress = gtk_progress_bar_new();
    gtk_widget_set_hexpand(m_rpmProgress, TRUE);
    gtk_widget_set_valign(m_rpmProgress, GTK_ALIGN_CENTER);
    double maxEstimate = (m_info.maxRpm > 0) ? m_info.maxRpm : 3500.0;
    double fraction = std::clamp(static_cast<double>(m_info.rpm) / maxEstimate, 0.0, 1.0);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(m_rpmProgress), fraction);
    gtk_box_append(GTK_BOX(row1), m_rpmProgress);

    // Type selector dropdown (compact)
    std::string opt0 = _("CPU");
    std::string opt1 = _("GPU");
    std::string opt2 = _("Chassis");
    std::string opt3 = _("Other");
    const char* const typeStrings[] = {
        opt0.c_str(),
        opt1.c_str(),
        opt2.c_str(),
        opt3.c_str(),
        nullptr
    };
    m_typeDropDown = gtk_drop_down_new_from_strings(typeStrings);
    guint selectedIdx = 2; // default external
    if (m_info.userType == FanType::CPU) selectedIdx = 0;
    else if (m_info.userType == FanType::GPU) selectedIdx = 1;
    else if (m_info.userType == FanType::External) selectedIdx = 2;
    else selectedIdx = 3;
    gtk_drop_down_set_selected(GTK_DROP_DOWN(m_typeDropDown), selectedIdx);
    g_signal_connect(m_typeDropDown, "notify::selected", G_CALLBACK(onTypeDropDownChanged), this);
    gtk_box_append(GTK_BOX(row1), m_typeDropDown);

    // Mode Selector (Auto / Simple / Curve)
    if (m_info.hasPwm) {
        std::string mode0 = _("Auto (BIOS)");
        std::string mode1 = _("Simple (Slider)");
        std::string mode2 = _("Curve");
        const char* const modeStrings[] = {
            mode0.c_str(),
            mode1.c_str(),
            mode2.c_str(),
            nullptr
        };
        m_modeDropDown = gtk_drop_down_new_from_strings(modeStrings);
        guint modeIdx = 0;
        if (m_info.controlMode == FanControlMode::Simple) modeIdx = 1;
        else if (m_info.controlMode == FanControlMode::Curve) modeIdx = 2;
        gtk_drop_down_set_selected(GTK_DROP_DOWN(m_modeDropDown), modeIdx);
        g_signal_connect(m_modeDropDown, "notify::selected", G_CALLBACK(onModeChanged), this);
        gtk_box_append(GTK_BOX(row1), m_modeDropDown);
    }

    if (m_info.hasPwm) {
        // --- 1. Simple Mode Box ---
        m_simpleBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_box_append(GTK_BOX(m_rootBox), m_simpleBox);

        GtkWidget* manualLabel = gtk_label_new(_("Speed:"));
        gtk_widget_add_css_class(manualLabel, "dim-label");
        gtk_box_append(GTK_BOX(m_simpleBox), manualLabel);

        m_speedScale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
        gtk_widget_set_hexpand(m_speedScale, TRUE);
        gtk_scale_set_draw_value(GTK_SCALE(m_speedScale), FALSE);
        gtk_range_set_value(GTK_RANGE(m_speedScale), m_info.currentPwmPercent);
        g_signal_connect(m_speedScale, "value-changed", G_CALLBACK(onScaleValueChanged), this);
        gtk_box_append(GTK_BOX(m_simpleBox), m_speedScale);

        m_speedPercentLabel = gtk_label_new((std::to_string(m_info.currentPwmPercent) + " %").c_str());
        gtk_widget_set_size_request(m_speedPercentLabel, 40, -1);
        gtk_box_append(GTK_BOX(m_simpleBox), m_speedPercentLabel);

        const int presets[] = {30, 50, 80, 100};
        m_speedPresetBtns.clear();
        for (int p : presets) {
            std::string pText = std::to_string(p) + "%";
            GtkWidget* pBtn = gtk_button_new_with_label(pText.c_str());
            gtk_widget_add_css_class(pBtn, "pill-btn");
            g_object_set_data(G_OBJECT(pBtn), "preset_value", GINT_TO_POINTER(p));
            g_signal_connect(pBtn, "clicked", G_CALLBACK(onPresetClicked), this);
            gtk_box_append(GTK_BOX(m_simpleBox), pBtn);
            m_speedPresetBtns.push_back({p, pBtn});
        }
        updateSpeedPresetHighlight(m_info.currentPwmPercent);

        m_applyBtn = gtk_button_new_with_label(_("Apply"));
        gtk_widget_add_css_class(m_applyBtn, "pill-btn");
        gtk_widget_add_css_class(m_applyBtn, "suggested-action");
        g_signal_connect(m_applyBtn, "clicked", G_CALLBACK(onApplyClicked), this);
        gtk_box_append(GTK_BOX(m_simpleBox), m_applyBtn);

        // --- 2. Curve Mode Box ---
        m_curveBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_add_css_class(m_curveBox, "curve-box");
        gtk_box_append(GTK_BOX(m_rootBox), m_curveBox);

        m_curveWidget = std::make_unique<FanCurveWidget>();
        m_curveWidget->setPoints(m_info.curvePoints);
        m_curveWidget->setOnPointsChangedCallback([this](const std::vector<CurvePoint>& pts) {
            m_info.curvePoints = pts;
            std::cout << "[KAIZEN GUI] Fan '" << m_info.id << "': curve edited by user (" << pts.size() << " control points)" << std::endl;
            if (m_dbusClient && m_dbusClient->isConnected()) {
                std::string msg;
                m_dbusClient->setFanCurve(m_info.id, JsonUtils::curvePointsToJson(pts), msg);
            } else if (m_fanMgr) {
                m_fanMgr->setFanCurvePoints(m_info.id, pts);
            }
        });
        gtk_box_append(GTK_BOX(m_curveBox), m_curveWidget->getWidget());

        // Curve presets and info row
        GtkWidget* curveActionRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_box_append(GTK_BOX(m_curveBox), curveActionRow);

        GtkWidget* presetTitle = gtk_label_new(_("Curve Preset:"));
        gtk_widget_add_css_class(presetTitle, "dim-label");
        gtk_box_append(GTK_BOX(curveActionRow), presetTitle);

        struct CurvePreset {
            const char* label;
            int type;
        };
        CurvePreset cPresets[] = {
            {_("Quiet"), 0},
            {_("Balanced"), 1},
            {_("Aggressive"), 2}
        };

        m_curvePresetBtns.clear();
        for (const auto& cp : cPresets) {
            GtkWidget* cpBtn = gtk_button_new_with_label(cp.label);
            gtk_widget_add_css_class(cpBtn, "pill-btn");
            g_object_set_data(G_OBJECT(cpBtn), "curve_preset", GINT_TO_POINTER(cp.type));
            g_signal_connect(cpBtn, "clicked", G_CALLBACK(onCurvePresetClicked), this);
            gtk_box_append(GTK_BOX(curveActionRow), cpBtn);
            m_curvePresetBtns.push_back({cp.type, cpBtn});
        }

        m_curveInfoLabel = gtk_label_new("");
        gtk_widget_set_hexpand(m_curveInfoLabel, TRUE);
        gtk_widget_set_halign(m_curveInfoLabel, GTK_ALIGN_END);
        gtk_widget_add_css_class(m_curveInfoLabel, "dim-label");
        gtk_box_append(GTK_BOX(curveActionRow), m_curveInfoLabel);

        // Update initial visibility
        updateModeVisibility(m_info.controlMode);
    }
}

void FanCard::updateData(const FanInfo& info, double currentTemp) {
    m_info = info;

    if (m_rpmValueLabel) {
        gtk_label_set_text(GTK_LABEL(m_rpmValueLabel), UIUtils::formatRpm(info.rpm).c_str());
    }

    if (m_rpmProgress) {
        double maxEstimate = (info.maxRpm > 0) ? info.maxRpm : 3500.0;
        double fraction = std::clamp(static_cast<double>(info.rpm) / maxEstimate, 0.0, 1.0);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(m_rpmProgress), fraction);
    }

    if (m_curveWidget && currentTemp > 0.0) {
        m_curveWidget->setCurrentTemp(currentTemp);
        if (m_curveInfoLabel) {
            int target = m_info.evaluateCurve(currentTemp);
            std::string infoStr = std::string(_("Temp: ")) + std::to_string(static_cast<int>(currentTemp)) + "°C → " +
                                  _("Target: ") + std::to_string(target) + "%";
            gtk_label_set_text(GTK_LABEL(m_curveInfoLabel), infoStr.c_str());
        }
    }

    if (m_info.hasPwm && !m_updatingFromModel) {
        m_updatingFromModel = true;

        if (m_speedPercentLabel && m_info.controlMode == FanControlMode::Simple) {
            gtk_label_set_text(GTK_LABEL(m_speedPercentLabel), (std::to_string(info.currentPwmPercent) + " %").c_str());
        }

        m_updatingFromModel = false;
    }
}

void FanCard::onModeChanged(GObject* obj, GParamSpec*, gpointer userData) {
    FanCard* self = static_cast<FanCard*>(userData);
    if (self->m_updatingFromModel) return;

    guint sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(obj));
    FanControlMode newMode = FanControlMode::Auto;
    if (sel == 1) newMode = FanControlMode::Simple;
    else if (sel == 2) newMode = FanControlMode::Curve;

    self->m_info.controlMode = newMode;
    std::cout << "[KAIZEN GUI] Fan '" << self->m_info.id << "': mode changed to "
              << (newMode == FanControlMode::Auto ? "Auto" : ((newMode == FanControlMode::Simple) ? "Simple (Manual)" : "Curve")) << std::endl;

    if (self->m_dbusClient && self->m_dbusClient->isConnected()) {
        std::string msg;
        self->m_dbusClient->setFanMode(self->m_info.id, fanControlModeToString(newMode), msg);
        if (newMode == FanControlMode::Simple && self->m_speedScale) {
            int val = static_cast<int>(std::round(gtk_range_get_value(GTK_RANGE(self->m_speedScale))));
            self->m_dbusClient->setFanSpeed(self->m_info.id, val, msg);
        } else if (newMode == FanControlMode::Curve) {
            self->m_dbusClient->setFanCurve(self->m_info.id, JsonUtils::curvePointsToJson(self->m_info.curvePoints), msg);
        }
    } else if (self->m_fanMgr) {
        self->m_fanMgr->setFanControlMode(self->m_info.id, newMode);
        if (newMode == FanControlMode::Simple && self->m_speedScale) {
            int val = static_cast<int>(std::round(gtk_range_get_value(GTK_RANGE(self->m_speedScale))));
            self->m_fanMgr->setFanSpeedPercent(self->m_info.id, val);
        } else if (newMode == FanControlMode::Curve) {
            self->m_fanMgr->setFanCurvePoints(self->m_info.id, self->m_info.curvePoints);
        }
    }
    self->updateModeVisibility(newMode);
}

void FanCard::updateSpeedPresetHighlight(int currentPercent) {
    for (auto& [pct, btn] : m_speedPresetBtns) {
        if (pct == currentPercent) {
            gtk_widget_add_css_class(btn, "suggested-action");
            gtk_widget_add_css_class(btn, "active");
        } else {
            gtk_widget_remove_css_class(btn, "suggested-action");
            gtk_widget_remove_css_class(btn, "active");
        }
    }
}

void FanCard::updateCurvePresetHighlight(int presetType) {
    for (auto& [pt, btn] : m_curvePresetBtns) {
        if (pt == presetType) {
            gtk_widget_add_css_class(btn, "suggested-action");
            gtk_widget_add_css_class(btn, "active");
        } else {
            gtk_widget_remove_css_class(btn, "suggested-action");
            gtk_widget_remove_css_class(btn, "active");
        }
    }
}

void FanCard::onScaleValueChanged(GtkRange* range, gpointer userData) {
    FanCard* self = static_cast<FanCard*>(userData);
    int val = static_cast<int>(std::round(gtk_range_get_value(range)));
    if (self->m_speedPercentLabel) {
        gtk_label_set_text(GTK_LABEL(self->m_speedPercentLabel), (std::to_string(val) + " %").c_str());
    }
    self->updateSpeedPresetHighlight(val);
}

void FanCard::onPresetClicked(GtkButton* btn, gpointer userData) {
    FanCard* self = static_cast<FanCard*>(userData);
    int p = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "preset_value"));
    std::cout << "[KAIZEN GUI] Fan '" << self->m_info.id << "': preset " << p << "% clicked" << std::endl;
    if (self->m_speedScale) {
        gtk_range_set_value(GTK_RANGE(self->m_speedScale), p);
    }
    self->updateSpeedPresetHighlight(p);
    if (self->m_dbusClient && self->m_dbusClient->isConnected()) {
        std::string msg;
        self->m_dbusClient->setFanSpeed(self->m_info.id, p, msg);
    } else if (self->m_fanMgr) {
        self->m_fanMgr->setFanSpeedPercent(self->m_info.id, p);
    }
}

void FanCard::onApplyClicked(GtkButton*, gpointer userData) {
    FanCard* self = static_cast<FanCard*>(userData);
    if (!self->m_speedScale) return;

    int percent = static_cast<int>(std::round(gtk_range_get_value(GTK_RANGE(self->m_speedScale))));
    std::cout << "[KAIZEN GUI] Fan '" << self->m_info.id << "': applied speed " << percent << "%" << std::endl;
    self->updateSpeedPresetHighlight(percent);
    if (self->m_dbusClient && self->m_dbusClient->isConnected()) {
        std::string msg;
        self->m_dbusClient->setFanSpeed(self->m_info.id, percent, msg);
    } else if (self->m_fanMgr) {
        self->m_fanMgr->setFanSpeedPercent(self->m_info.id, percent);
    }
}

void FanCard::onTypeDropDownChanged(GObject* obj, GParamSpec*, gpointer userData) {
    FanCard* self = static_cast<FanCard*>(userData);
    guint sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(obj));
    FanType newType = FanType::External;
    if (sel == 0) newType = FanType::CPU;
    else if (sel == 1) newType = FanType::GPU;
    else if (sel == 2) newType = FanType::External;
    else newType = FanType::Unknown;

    std::cout << "[KAIZEN GUI] Fan '" << self->m_info.id << "': user changed type to "
              << ((newType == FanType::CPU) ? "CPU" : ((newType == FanType::GPU) ? "GPU" : "Chassis")) << std::endl;

    self->m_info.userType = newType;
    if (self->m_fanMgr) {
        self->m_fanMgr->setUserFanType(self->m_info.id, newType);
    }
    self->updateBadge(newType);
}

void FanCard::onCurvePresetClicked(GtkButton* btn, gpointer userData) {
    FanCard* self = static_cast<FanCard*>(userData);
    int presetType = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "curve_preset"));
    std::string presetName = (presetType == 0) ? "Quiet" : ((presetType == 1) ? "Balanced" : "Aggressive");
    std::cout << "[KAIZEN GUI] Fan '" << self->m_info.id << "': applied curve preset " << presetName << std::endl;
    self->updateCurvePresetHighlight(presetType);

    std::vector<CurvePoint> pts;
    if (presetType == 0) {
        // Quiet
        pts = {{25, 20}, {50, 35}, {70, 60}, {85, 100}};
    } else if (presetType == 1) {
        // Balanced
        pts = {{30, 25}, {50, 45}, {70, 75}, {85, 100}};
    } else {
        // Aggressive
        pts = {{25, 40}, {45, 65}, {65, 90}, {80, 100}};
    }

    self->m_info.curvePoints = pts;
    if (self->m_curveWidget) {
        self->m_curveWidget->setPoints(pts);
    }
    if (self->m_dbusClient && self->m_dbusClient->isConnected()) {
        std::string msg;
        self->m_dbusClient->setFanCurve(self->m_info.id, JsonUtils::curvePointsToJson(pts), msg);
    } else if (self->m_fanMgr) {
        self->m_fanMgr->setFanCurvePoints(self->m_info.id, pts);
    }
}

} // namespace fc
