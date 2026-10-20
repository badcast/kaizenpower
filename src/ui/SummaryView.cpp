#include "SummaryView.hpp"
#include "UIUtils.hpp"
#include "../core/SystemInfo.hpp"
#include "../core/I18n.hpp"
#include <iomanip>
#include <sstream>

namespace fc {

SummaryView::SummaryView(std::shared_ptr<CpuManager> cpuMgr,
                         std::shared_ptr<GpuManager> gpuMgr,
                         std::shared_ptr<FanManager> fanMgr,
                         std::shared_ptr<ProfileManager> profileMgr,
                         std::shared_ptr<DBusClient> dbusClient)
    : m_cpuMgr(std::move(cpuMgr)),
      m_gpuMgr(std::move(gpuMgr)),
      m_fanMgr(std::move(fanMgr)),
      m_profileMgr(std::move(profileMgr)),
      m_dbusClient(std::move(dbusClient)) {
    buildUI();
}

void SummaryView::buildUI() {
    m_rootBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(m_rootBox, 10);
    gtk_widget_set_margin_end(m_rootBox, 10);
    gtk_widget_set_margin_top(m_rootBox, 8);
    gtk_widget_set_margin_bottom(m_rootBox, 8);

    OsInfo os = SystemInfo::getOsInfo();

    // 1. Ultra-compact OS Branding & System Details Banner Card
    GtkWidget* osCard = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(osCard, "fan-card");
    gtk_box_append(GTK_BOX(m_rootBox), osCard);

    // OS Logo Icon (size 32px)
    GtkIconTheme* theme = gtk_icon_theme_get_for_display(gdk_display_get_default());
    std::string iconName = os.logo;
    if (!gtk_icon_theme_has_icon(theme, iconName.c_str())) {
        if (gtk_icon_theme_has_icon(theme, "distributor-logo")) {
            iconName = "distributor-logo";
        } else {
            iconName = "preferences-desktop-display";
        }
    }
    m_osLogoImage = gtk_image_new_from_icon_name(iconName.c_str());
    gtk_image_set_pixel_size(GTK_IMAGE(m_osLogoImage), 32);
    gtk_box_append(GTK_BOX(osCard), m_osLogoImage);

    // OS Title and Details in a compact row
    GtkWidget* osDetailsVBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(osDetailsVBox, TRUE);
    gtk_box_append(GTK_BOX(osCard), osDetailsVBox);

    GtkWidget* titleRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(osDetailsVBox), titleRow);

    m_osNameLabel = gtk_label_new(nullptr);
    gtk_widget_set_halign(m_osNameLabel, GTK_ALIGN_START);
    std::string titleMarkup = "<span font_weight='bold' font_size='11pt'>" + os.prettyName + "</span>";
    gtk_label_set_markup(GTK_LABEL(m_osNameLabel), titleMarkup.c_str());
    gtk_box_append(GTK_BOX(titleRow), m_osNameLabel);

    // Badges inline
    m_kernelBadge = UIUtils::createBadge("Kernel: " + os.kernel, "fan-badge-case");
    gtk_box_append(GTK_BOX(titleRow), m_kernelBadge);

    m_archBadge = UIUtils::createBadge("Arch: " + os.architecture, "fan-badge-cpu");
    gtk_box_append(GTK_BOX(titleRow), m_archBadge);

    m_uptimeBadge = UIUtils::createBadge("Uptime: " + os.uptimeStr, "fan-badge-gpu");
    gtk_box_append(GTK_BOX(titleRow), m_uptimeBadge);

    // Daemon Status Indicator
    bool daemonUp = m_dbusClient && m_dbusClient->isConnected();
    m_daemonStatusBadge = UIUtils::createBadge(daemonUp ? "DAEMON: ACTIVE (D-Bus)" : "DAEMON: LOCAL (Polkit)",
                                              daemonUp ? "privilege-badge-root" : "privilege-badge-user");
    gtk_widget_set_valign(m_daemonStatusBadge, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(osCard), m_daemonStatusBadge);

    // Kaizen Power Tool Application Logo
    std::string logoPath = UIUtils::getLogoPath();
    if (!logoPath.empty()) {
        GtkWidget* logoImg = gtk_image_new_from_file(logoPath.c_str());
        gtk_image_set_pixel_size(GTK_IMAGE(logoImg), 32);
        m_appLogoBtn = gtk_button_new();
        gtk_button_set_child(GTK_BUTTON(m_appLogoBtn), logoImg);
        gtk_button_set_has_frame(GTK_BUTTON(m_appLogoBtn), FALSE);
        gtk_widget_set_valign(m_appLogoBtn, GTK_ALIGN_CENTER);
        gtk_widget_set_tooltip_text(m_appLogoBtn, _("About Kaizen Power Tool"));
        g_signal_connect(m_appLogoBtn, "clicked", G_CALLBACK(onAboutClicked), this);
        gtk_box_append(GTK_BOX(osCard), m_appLogoBtn);
    }

    // 2. Hardware Overview Cards Grid (CPU, GPU, Fans)
    GtkWidget* grid = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_set_homogeneous(GTK_BOX(grid), TRUE);
    gtk_box_append(GTK_BOX(m_rootBox), grid);

    // CPU Card
    GtkWidget* cpuTile = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(cpuTile, "metric-box");
    gtk_box_append(GTK_BOX(grid), cpuTile);

    GtkWidget* cpuHdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(cpuTile), cpuHdr);
    GtkWidget* cpuIcon = gtk_image_new_from_icon_name("computer-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(cpuIcon), 15);
    gtk_box_append(GTK_BOX(cpuHdr), cpuIcon);
    GtkWidget* cpuTitle = gtk_label_new(_("Processor (CPU)"));
    gtk_widget_add_css_class(cpuTitle, "metric-title");
    gtk_box_append(GTK_BOX(cpuHdr), cpuTitle);

    m_cpuTempBadge = UIUtils::createBadge("-- °C", "fan-badge-cpu");
    gtk_widget_set_hexpand(m_cpuTempBadge, TRUE);
    gtk_widget_set_halign(m_cpuTempBadge, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(cpuHdr), m_cpuTempBadge);

    m_cpuModelLabel = gtk_label_new("Loading CPU...");
    gtk_widget_set_halign(m_cpuModelLabel, GTK_ALIGN_START);
    gtk_label_set_ellipsize(GTK_LABEL(m_cpuModelLabel), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(m_cpuModelLabel, "compact-title");
    gtk_box_append(GTK_BOX(cpuTile), m_cpuModelLabel);

    m_cpuSubLabel = gtk_label_new("Threads & Governor");
    gtk_widget_set_halign(m_cpuSubLabel, GTK_ALIGN_START);
    gtk_widget_add_css_class(m_cpuSubLabel, "dim-label");
    gtk_box_append(GTK_BOX(cpuTile), m_cpuSubLabel);

    // GPU Card
    GtkWidget* gpuTile = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(gpuTile, "metric-box");
    gtk_box_append(GTK_BOX(grid), gpuTile);

    GtkWidget* gpuHdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(gpuTile), gpuHdr);
    GtkWidget* gpuIcon = gtk_image_new_from_icon_name("show-gpu-effects-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(gpuIcon), 15);
    gtk_box_append(GTK_BOX(gpuHdr), gpuIcon);
    GtkWidget* gpuTitle = gtk_label_new(_("Graphics (GPU)"));
    gtk_widget_add_css_class(gpuTitle, "metric-title");
    gtk_box_append(GTK_BOX(gpuHdr), gpuTitle);

    m_gpuTempBadge = UIUtils::createBadge("-- °C", "fan-badge-gpu");
    gtk_widget_set_hexpand(m_gpuTempBadge, TRUE);
    gtk_widget_set_halign(m_gpuTempBadge, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(gpuHdr), m_gpuTempBadge);

    m_gpuModelLabel = gtk_label_new("Loading GPU...");
    gtk_widget_set_halign(m_gpuModelLabel, GTK_ALIGN_START);
    gtk_label_set_ellipsize(GTK_LABEL(m_gpuModelLabel), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(m_gpuModelLabel, "compact-title");
    gtk_box_append(GTK_BOX(gpuTile), m_gpuModelLabel);

    m_gpuSubLabel = gtk_label_new("Power & Clocks");
    gtk_widget_set_halign(m_gpuSubLabel, GTK_ALIGN_START);
    gtk_widget_add_css_class(m_gpuSubLabel, "dim-label");
    gtk_box_append(GTK_BOX(gpuTile), m_gpuSubLabel);

    // Fans Card
    GtkWidget* fanTile = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(fanTile, "metric-box");
    gtk_box_append(GTK_BOX(grid), fanTile);

    GtkWidget* fanHdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(fanTile), fanHdr);
    GtkWidget* fanIcon = gtk_image_new_from_icon_name("applications-system-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(fanIcon), 15);
    gtk_box_append(GTK_BOX(fanHdr), fanIcon);
    GtkWidget* fanTitle = gtk_label_new(_("Cooling Fans"));
    gtk_widget_add_css_class(fanTitle, "metric-title");
    gtk_box_append(GTK_BOX(fanHdr), fanTitle);

    m_fanSpeedBadge = UIUtils::createBadge("-- RPM", "fan-badge-case");
    gtk_widget_set_hexpand(m_fanSpeedBadge, TRUE);
    gtk_widget_set_halign(m_fanSpeedBadge, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(fanHdr), m_fanSpeedBadge);

    m_fanSummaryLabel = gtk_label_new("Detecting fans...");
    gtk_widget_set_halign(m_fanSummaryLabel, GTK_ALIGN_START);
    gtk_widget_add_css_class(m_fanSummaryLabel, "compact-title");
    gtk_box_append(GTK_BOX(fanTile), m_fanSummaryLabel);

    m_fanSubLabel = gtk_label_new("Active / Total");
    gtk_widget_set_halign(m_fanSubLabel, GTK_ALIGN_START);
    gtk_widget_add_css_class(m_fanSubLabel, "dim-label");
    gtk_box_append(GTK_BOX(fanTile), m_fanSubLabel);

    // 3. Compact Quick System Actions Bar (replaces tall preferences group)
    GtkWidget* actionCard = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(actionCard, "fan-card");
    gtk_box_append(GTK_BOX(m_rootBox), actionCard);

    GtkWidget* actIcon = gtk_image_new_from_icon_name("system-run-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(actIcon), 16);
    gtk_box_append(GTK_BOX(actionCard), actIcon);

    GtkWidget* actLabel = gtk_label_new(_("Quick Profiles:"));
    gtk_widget_add_css_class(actLabel, "metric-title");
    gtk_box_append(GTK_BOX(actionCard), actLabel);

    m_autoBtn = gtk_button_new_with_label(_("Factory Auto"));
    gtk_widget_add_css_class(m_autoBtn, "pill-btn");
    gtk_widget_add_css_class(m_autoBtn, "suggested-action");
    gtk_widget_add_css_class(m_autoBtn, "active");
    g_signal_connect(m_autoBtn, "clicked", G_CALLBACK(onAutoClicked), this);
    gtk_box_append(GTK_BOX(actionCard), m_autoBtn);

    struct ProfBtn {
        const char* label;
        SystemProfile prof;
    };
    ProfBtn profs[] = {
        {_("Quiet"), SystemProfile::Silent},
        {_("Balanced"), SystemProfile::Balanced},
        {_("Performance"), SystemProfile::Performance},
        {_("Max Cooling"), SystemProfile::MaxCooling}
    };
    m_profileBtns.clear();
    for (const auto& p : profs) {
        GtkWidget* b = gtk_button_new_with_label(p.label);
        gtk_widget_add_css_class(b, "pill-btn");
        g_object_set_data(G_OBJECT(b), "prof_type", GINT_TO_POINTER(static_cast<int>(p.prof)));
        g_signal_connect(b, "clicked", G_CALLBACK(onProfileClicked), this);
        gtk_box_append(GTK_BOX(actionCard), b);
        m_profileBtns.push_back({p.prof, b});
    }
}

void SummaryView::updateData(const CpuInfo& cpu,
                             const GpuInfo& gpu,
                             const std::vector<FanInfo>& fans,
                             const std::string&) {
    // 1. Update CPU Tile
    if (m_cpuModelLabel) {
        gtk_label_set_text(GTK_LABEL(m_cpuModelLabel), cpu.modelName.c_str());
    }
    if (m_cpuTempBadge) {
        gtk_label_set_text(GTK_LABEL(m_cpuTempBadge), (std::to_string(static_cast<int>(cpu.temperatureC)) + " °C").c_str());
    }
    if (m_cpuSubLabel) {
        std::string sub = std::to_string(cpu.coreCount) + " " + _("Cores") + " • " +
                          UIUtils::formatFrequency(cpu.avgFreqGhz) + " • " + cpu.currentGovernor;
        gtk_label_set_text(GTK_LABEL(m_cpuSubLabel), sub.c_str());
    }

    // 2. Update GPU Tile
    if (m_gpuModelLabel) {
        gtk_label_set_text(GTK_LABEL(m_gpuModelLabel), gpu.present ? gpu.name.c_str() : _("Integrated / Unknown GPU"));
    }
    if (m_gpuTempBadge) {
        gtk_label_set_text(GTK_LABEL(m_gpuTempBadge), gpu.present ? (std::to_string(static_cast<int>(gpu.temperatureC)) + " °C").c_str() : "N/A");
    }
    if (m_gpuSubLabel) {
        std::string sub = gpu.present ? (gpu.driver + " • " + std::to_string(static_cast<int>(gpu.powerW)) + " W • SCLK " + std::to_string(gpu.curSclkMhz) + "MHz")
                                      : _("Standard display driver");
        gtk_label_set_text(GTK_LABEL(m_gpuSubLabel), sub.c_str());
    }

    // 3. Update Fans Tile
    int spinning = 0, sumRpm = 0;
    for (const auto& f : fans) {
        if (f.rpm > 0) {
            spinning++;
            sumRpm += f.rpm;
        }
    }
    int avgRpm = (spinning > 0) ? (sumRpm / spinning) : 0;

    if (m_fanSummaryLabel) {
        std::string fanSummary = std::to_string(spinning) + " " + _("Active") + " / " + std::to_string(fans.size()) + " " + _("Total");
        gtk_label_set_text(GTK_LABEL(m_fanSummaryLabel), fanSummary.c_str());
    }
    if (m_fanSpeedBadge) {
        gtk_label_set_text(GTK_LABEL(m_fanSpeedBadge), (std::to_string(avgRpm) + " RPM").c_str());
    }
    if (m_fanSubLabel) {
        gtk_label_set_text(GTK_LABEL(m_fanSubLabel), _("Hardware telemetry normal"));
    }

    // 4. Update Uptime Badge
    if (m_uptimeBadge) {
        OsInfo os = SystemInfo::getOsInfo();
        gtk_label_set_text(GTK_LABEL(m_uptimeBadge), ("Uptime: " + os.uptimeStr).c_str());
    }
}

void SummaryView::updateActiveProfile(SystemProfile profile, bool isAuto) {
    if (m_autoBtn) {
        if (isAuto) {
            gtk_widget_add_css_class(m_autoBtn, "suggested-action");
            gtk_widget_add_css_class(m_autoBtn, "active");
        } else {
            gtk_widget_remove_css_class(m_autoBtn, "suggested-action");
            gtk_widget_remove_css_class(m_autoBtn, "active");
        }
    }

    for (auto& [p, btn] : m_profileBtns) {
        if (!isAuto && p == profile) {
            gtk_widget_add_css_class(btn, "suggested-action");
            gtk_widget_add_css_class(btn, "active");
        } else {
            gtk_widget_remove_css_class(btn, "suggested-action");
            gtk_widget_remove_css_class(btn, "active");
        }
    }
}

void SummaryView::onAutoClicked(GtkButton*, gpointer userData) {
    SummaryView* self = static_cast<SummaryView*>(userData);
    self->updateActiveProfile(SystemProfile::Balanced, true);
    if (self->m_onAutoAll) {
        self->m_onAutoAll();
    }
}

void SummaryView::onProfileClicked(GtkButton* btn, gpointer userData) {
    SummaryView* self = static_cast<SummaryView*>(userData);
    for (const auto& [prof, widget] : self->m_profileBtns) {
        if (widget == GTK_WIDGET(btn)) {
            self->updateActiveProfile(prof, false);
            if (self->m_onProfile) {
                self->m_onProfile(prof);
            }
            return;
        }
    }
}

void SummaryView::onAboutClicked(GtkButton*, gpointer userData) {
    SummaryView* self = static_cast<SummaryView*>(userData);
    if (self && self->m_onAbout) {
        self->m_onAbout();
    }
}

} // namespace fc
