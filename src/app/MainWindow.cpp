#include "MainWindow.hpp"
#include "../core/SysfsUtils.hpp"
#include "../core/I18n.hpp"
#include "../core/PrivilegeManager.hpp"
#include "../ui/UIUtils.hpp"

#include <iostream>

namespace fc {

MainWindow::MainWindow(AdwApplication* app, const std::string& defaultTab)
    : m_app(app) {
    // Initialize hardware managers
    m_fanMgr = std::make_shared<FanManager>();
    m_cpuMgr = std::make_shared<CpuManager>();
    m_gpuMgr = std::make_shared<GpuManager>();
    m_profileMgr = std::make_shared<ProfileManager>(m_fanMgr, m_cpuMgr, m_gpuMgr);
    m_dbusClient = std::make_shared<DBusClient>();
    m_dbusClient->connect();

    buildUI();

    if (!defaultTab.empty()) {
        GtkWidget* content = adw_toast_overlay_get_child(m_toastOverlay);
        if (ADW_IS_TOOLBAR_VIEW(content)) {
            GtkWidget* stack = adw_toolbar_view_get_content(ADW_TOOLBAR_VIEW(content));
            if (ADW_IS_VIEW_STACK(stack)) {
                adw_view_stack_set_visible_child_name(ADW_VIEW_STACK(stack), defaultTab.c_str());
            }
        }
    }

    // Start 1-second periodic timer for live hardware updates
    m_timerId = g_timeout_add_seconds(1, onTimerTick, this);
}

MainWindow::~MainWindow() {
    if (m_timerId > 0) {
        g_source_remove(m_timerId);
        m_timerId = 0;
    }
}

void MainWindow::show() {
    if (m_window) {
        gtk_window_present(GTK_WINDOW(m_window));
    }
}

void MainWindow::showToast(const std::string& message) {
    if (m_toastOverlay) {
        AdwToast* toast = adw_toast_new(message.c_str());
        adw_toast_set_timeout(toast, 2);
        adw_toast_overlay_add_toast(m_toastOverlay, toast);
    }
}

void MainWindow::buildUI() {
    m_window = ADW_APPLICATION_WINDOW(adw_application_window_new(GTK_APPLICATION(m_app)));
    gtk_window_set_default_size(GTK_WINDOW(m_window), 780, 500);
    gtk_window_set_title(GTK_WINDOW(m_window), "Kaizen Power Tool — Hardware & Fan Controller");

    m_toastOverlay = ADW_TOAST_OVERLAY(adw_toast_overlay_new());
    adw_application_window_set_content(m_window, GTK_WIDGET(m_toastOverlay));

    // ToolbarView
    AdwToolbarView* toolbarView = ADW_TOOLBAR_VIEW(adw_toolbar_view_new());
    adw_toast_overlay_set_child(m_toastOverlay, GTK_WIDGET(toolbarView));

    // HeaderBar
    AdwHeaderBar* headerBar = ADW_HEADER_BAR(adw_header_bar_new());
    adw_toolbar_view_add_top_bar(toolbarView, GTK_WIDGET(headerBar));

    // ViewStack
    AdwViewStack* viewStack = ADW_VIEW_STACK(adw_view_stack_new());

    // Switcher in HeaderBar
    AdwViewSwitcher* switcher = ADW_VIEW_SWITCHER(adw_view_switcher_new());
    adw_view_switcher_set_stack(switcher, viewStack);
    adw_view_switcher_set_policy(switcher, ADW_VIEW_SWITCHER_POLICY_WIDE);
    adw_header_bar_set_title_widget(headerBar, GTK_WIDGET(switcher));

    // Header buttons (Left: Privilege badge & Language dropdown)
    bool isRoot = SysfsUtils::isRoot();
    bool daemonConnected = m_dbusClient && m_dbusClient->isConnected();
    bool hasHardwareControl = isRoot || daemonConnected;

    std::string badgeText = isRoot ? "ROOT" : (daemonConnected ? "DAEMON" : _("READ ONLY"));
    std::string badgeClass = hasHardwareControl ? "privilege-badge-root" : "privilege-badge-user";

    m_privBadge = UIUtils::createBadge(badgeText, badgeClass);
    adw_header_bar_pack_start(headerBar, m_privBadge);

    m_elevateBtn = gtk_button_new_from_icon_name("system-lock-screen-symbolic");
    gtk_widget_set_tooltip_text(m_elevateBtn, _("Restart with administrator privileges (pkexec) to enable hardware control"));
    g_signal_connect(m_elevateBtn, "clicked", G_CALLBACK(onElevateClicked), this);
    gtk_widget_set_visible(m_elevateBtn, !hasHardwareControl);
    adw_header_bar_pack_start(headerBar, m_elevateBtn);

    // Language Dropdown in HeaderBar
    const auto& langs = i18n::getSupportedLanguages();
    std::vector<const char*> langStrings;
    int selectedLangIdx = 0;
    std::string currentLang = i18n::getCurrentLanguage();
    for (size_t i = 0; i < langs.size(); ++i) {
        langStrings.push_back(langs[i].displayName.c_str());
        if (langs[i].code == currentLang) {
            selectedLangIdx = static_cast<int>(i);
        }
    }
    langStrings.push_back(nullptr);

    m_langDropDown = gtk_drop_down_new_from_strings(langStrings.data());
    gtk_drop_down_set_selected(GTK_DROP_DOWN(m_langDropDown), selectedLangIdx);
    gtk_widget_set_tooltip_text(m_langDropDown, _("Change interface language"));
    g_signal_connect(m_langDropDown, "notify::selected", G_CALLBACK(onLanguageChanged), this);
    adw_header_bar_pack_end(headerBar, m_langDropDown);

    GtkWidget* refreshBtn = gtk_button_new_from_icon_name("view-refresh-symbolic");
    gtk_widget_set_tooltip_text(refreshBtn, _("Refresh sensor data"));
    g_signal_connect(refreshBtn, "clicked", G_CALLBACK(onRefreshClicked), this);
    adw_header_bar_pack_end(headerBar, refreshBtn);

    m_aboutBtn = gtk_button_new_from_icon_name("help-about-symbolic");
    gtk_widget_set_tooltip_text(m_aboutBtn, _("About Kaizen Power Tool"));
    g_signal_connect(m_aboutBtn, "clicked", G_CALLBACK(onAboutClicked), this);
    adw_header_bar_pack_end(headerBar, m_aboutBtn);

    // Banner for non-root users without active daemon
    if (!hasHardwareControl) {
        m_privilegeBanner = ADW_BANNER(adw_banner_new(_("The application is running in read-only mode. Administrator (Root) privileges are required to modify frequencies and fan speeds.")));
        adw_banner_set_button_label(m_privilegeBanner, _("Elevate Privileges"));
        adw_banner_set_revealed(m_privilegeBanner, TRUE);
        g_signal_connect(m_privilegeBanner, "button-clicked", G_CALLBACK(onBannerButtonClicked), this);
        adw_toolbar_view_add_top_bar(toolbarView, GTK_WIDGET(m_privilegeBanner));
    }

    // --- PAGE 1: SUMMARY / DASHBOARD ---
    GtkWidget* summaryScroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(summaryScroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    AdwClamp* summaryClamp = ADW_CLAMP(adw_clamp_new());
    adw_clamp_set_maximum_size(summaryClamp, 800);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(summaryScroll), GTK_WIDGET(summaryClamp));

    m_summaryView = std::make_unique<SummaryView>(m_cpuMgr, m_gpuMgr, m_fanMgr, m_profileMgr, m_dbusClient);
    m_summaryView->setOnAutoAllCallback([this]() {
        std::cout << "[KAIZEN GUI] User activated Factory Auto mode" << std::endl;
        std::string outMsg;
        if (m_dbusClient && m_dbusClient->isConnected()) {
            m_dbusClient->setAuto("all", outMsg);
        } else {
            if (m_profileMgr) m_profileMgr->restoreAuto();
            outMsg = _("Hardware restored to factory automatic mode");
        }
        if (m_profileMgr) m_profileMgr->setAuto(true);
        if (m_summaryView && m_profileMgr) {
            m_summaryView->updateActiveProfile(m_profileMgr->getCurrentProfile(), true);
        }
        if (m_profilesView && m_profileMgr) {
            m_profilesView->updateActiveState(m_profileMgr->getCurrentProfile());
        }
        showToast(outMsg);
        refreshData();
    });
    m_summaryView->setOnProfileCallback([this](SystemProfile p) {
        std::string profEn = m_profileMgr ? m_profileMgr->getProfileNameEnglish(p) : "Unknown";
        std::cout << "[KAIZEN GUI] User activated profile: " << profEn << std::endl;
        if (m_dbusClient && m_dbusClient->isConnected()) {
            std::string pName = (p == SystemProfile::Silent) ? "silent" :
                                ((p == SystemProfile::Performance) ? "performance" :
                                ((p == SystemProfile::MaxCooling) ? "cooling" : "balanced"));
            std::string msg;
            m_dbusClient->applyProfile(pName, msg);
        } else {
            if (m_profileMgr) m_profileMgr->applyProfile(p);
        }
        if (m_profileMgr) {
            m_profileMgr->setActiveProfile(p);
            m_profileMgr->setAuto(false);
        }
        if (m_summaryView && m_profileMgr) {
            m_summaryView->updateActiveProfile(p, false);
        }
        if (m_profilesView && m_profileMgr) {
            m_profilesView->updateActiveState(p);
        }
        showToast(std::string(_("Profile activated: ")) + m_profileMgr->getProfileName(p));
        refreshData();
    });
    m_summaryView->setOnAboutCallback([this]() {
        showAboutDialog();
    });
    adw_clamp_set_child(summaryClamp, m_summaryView->getWidget());
    adw_view_stack_add_titled_with_icon(viewStack, summaryScroll, "summary", _("Overview"), "computer-symbolic");

    // --- PAGE 2: FANS ---
    GtkWidget* fansScroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(fansScroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    AdwClamp* fansClamp = ADW_CLAMP(adw_clamp_new());
    adw_clamp_set_maximum_size(fansClamp, 800);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(fansScroll), GTK_WIDGET(fansClamp));

    m_fansContainer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(m_fansContainer, 12);
    gtk_widget_set_margin_end(m_fansContainer, 12);
    gtk_widget_set_margin_top(m_fansContainer, 10);
    gtk_widget_set_margin_bottom(m_fansContainer, 10);
    adw_clamp_set_child(fansClamp, m_fansContainer);

    // Fan Summary Banner
    m_fanSummaryLabel = gtk_label_new("");
    gtk_widget_set_halign(m_fanSummaryLabel, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(m_fansContainer), m_fanSummaryLabel);

    // Build Fan Cards
    const auto& fans = m_fanMgr->getFans();
    int cpuCount = 0, gpuCount = 0, caseCount = 0, spinningCount = 0;
    for (const auto& fan : fans) {
        if (fan.rpm > 0) spinningCount++;
        if (fan.userType == FanType::CPU) cpuCount++;
        else if (fan.userType == FanType::GPU) gpuCount++;
        else if (fan.userType == FanType::External) caseCount++;

        auto card = std::make_unique<FanCard>(fan, m_fanMgr, m_dbusClient);
        gtk_box_append(GTK_BOX(m_fansContainer), card->getWidget());
        m_fanCards.push_back(std::move(card));
    }

    std::string summaryText = "<b>" + std::string(_("Detected fans: ")) + std::to_string(fans.size()) +
                              "</b> (" + std::string(_("Active: ")) + std::to_string(spinningCount) +
                              ") • CPU: " + std::to_string(cpuCount) +
                              " • GPU: " + std::to_string(gpuCount) +
                              " • " + std::string(_("Chassis: ")) + std::to_string(caseCount);
    gtk_label_set_markup(GTK_LABEL(m_fanSummaryLabel), summaryText.c_str());

    adw_view_stack_add_titled_with_icon(viewStack, fansScroll, "fans", _("Fans"), "system-run-symbolic");

    // --- PAGE 2: CPU ---
    GtkWidget* cpuScroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(cpuScroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    AdwClamp* cpuClamp = ADW_CLAMP(adw_clamp_new());
    adw_clamp_set_maximum_size(cpuClamp, 800);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(cpuScroll), GTK_WIDGET(cpuClamp));

    m_cpuView = std::make_unique<CpuView>(m_cpuMgr, m_dbusClient);
    adw_clamp_set_child(cpuClamp, m_cpuView->getWidget());
    adw_view_stack_add_titled_with_icon(viewStack, cpuScroll, "cpu", _("Processor"), "computer-symbolic");

    // --- PAGE 3: GPU ---
    GtkWidget* gpuScroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(gpuScroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    AdwClamp* gpuClamp = ADW_CLAMP(adw_clamp_new());
    adw_clamp_set_maximum_size(gpuClamp, 800);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(gpuScroll), GTK_WIDGET(gpuClamp));

    m_gpuView = std::make_unique<GpuView>(m_gpuMgr, m_dbusClient);
    adw_clamp_set_child(gpuClamp, m_gpuView->getWidget());
    adw_view_stack_add_titled_with_icon(viewStack, gpuScroll, "gpu", _("Graphics"), "show-gpu-effects-symbolic");

    // --- PAGE 4: PROFILES ---
    GtkWidget* profScroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(profScroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    AdwClamp* profClamp = ADW_CLAMP(adw_clamp_new());
    adw_clamp_set_maximum_size(profClamp, 800);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(profScroll), GTK_WIDGET(profClamp));

    m_profilesView = std::make_unique<ProfilesView>(m_profileMgr);
    m_profilesView->setOnProfileAppliedCallback([this](SystemProfile p) {
        std::string profEn = m_profileMgr ? m_profileMgr->getProfileNameEnglish(p) : "Unknown";
        std::cout << "[KAIZEN GUI] User activated profile (Profiles Tab): " << profEn << std::endl;
        if (m_dbusClient && m_dbusClient->isConnected()) {
            std::string pName = (p == SystemProfile::Silent) ? "silent" :
                                ((p == SystemProfile::Performance) ? "performance" :
                                ((p == SystemProfile::MaxCooling) ? "cooling" : "balanced"));
            std::string msg;
            m_dbusClient->applyProfile(pName, msg);
        } else {
            if (m_profileMgr) m_profileMgr->applyProfile(p);
        }
        if (m_profileMgr) {
            m_profileMgr->setActiveProfile(p);
            m_profileMgr->setAuto(false);
        }
        if (m_summaryView && m_profileMgr) {
            m_summaryView->updateActiveProfile(p, false);
        }
        if (m_profilesView && m_profileMgr) {
            m_profilesView->updateActiveState(p);
        }
        showToast(std::string(_("Profile activated: ")) + m_profileMgr->getProfileName(p));
        refreshData();
    });
    adw_clamp_set_child(profClamp, m_profilesView->getWidget());
    adw_view_stack_add_titled_with_icon(viewStack, profScroll, "profiles", _("Profiles"), "preferences-system-symbolic");

    // Set ViewStack as content of ToolbarView
    adw_toolbar_view_set_content(toolbarView, GTK_WIDGET(viewStack));
}

void MainWindow::refreshData() {
    double cpuTemp = 0.0;
    if (m_cpuMgr && m_cpuView) {
        m_cpuMgr->updateLiveMetrics();
        const auto& cpuInfo = m_cpuMgr->getCpuInfo();
        cpuTemp = cpuInfo.temperatureC;
        m_cpuView->updateData(cpuInfo);
    }

    double gpuTemp = 0.0;
    if (m_gpuMgr && m_gpuView) {
        m_gpuMgr->updateLiveMetrics();
        const auto& gpuInfo = m_gpuMgr->getGpuInfo();
        gpuTemp = gpuInfo.temperatureC;
        m_gpuView->updateData(gpuInfo);
    }

    if (m_fanMgr) {
        m_fanMgr->updateLiveMetrics(cpuTemp, gpuTemp);
        const auto& fans = m_fanMgr->getFans();
        int spinningCount = 0;
        for (const auto& fan : fans) {
            if (fan.rpm > 0) spinningCount++;
            double fanTemp = (fan.userType == FanType::GPU) ? gpuTemp : cpuTemp;
            for (auto& card : m_fanCards) {
                if (card->getFanId() == fan.id) {
                    card->updateData(fan, fanTemp);
                    break;
                }
            }
        }
        if (m_fanSummaryLabel) {
            std::string summaryText = "<b>" + std::string(_("Detected fans: ")) + std::to_string(fans.size()) +
                                      "</b> (" + std::string(_("Active: ")) + std::to_string(spinningCount) + ")";
            gtk_label_set_markup(GTK_LABEL(m_fanSummaryLabel), summaryText.c_str());
        }
    }

    if (m_summaryView && m_cpuMgr && m_gpuMgr && m_fanMgr && m_profileMgr) {
        m_summaryView->updateData(m_cpuMgr->getCpuInfo(),
                                  m_gpuMgr->getGpuInfo(),
                                  m_fanMgr->getFans(),
                                  m_profileMgr->getProfileName(m_profileMgr->getCurrentProfile()));
        m_summaryView->updateActiveProfile(m_profileMgr->getCurrentProfile(), m_profileMgr->isAuto());
    }
    if (m_profilesView && m_profileMgr) {
        m_profilesView->updateActiveState(m_profileMgr->getCurrentProfile());
    }
}

gboolean MainWindow::onTimerTick(gpointer userData) {
    MainWindow* self = static_cast<MainWindow*>(userData);
    if (self->m_livePolling) {
        self->refreshData();
    }
    return G_SOURCE_CONTINUE;
}

void MainWindow::onRefreshClicked(GtkButton*, gpointer userData) {
    MainWindow* self = static_cast<MainWindow*>(userData);
    self->refreshData();
    self->showToast(_("Sensors data updated"));
}

void MainWindow::showAboutDialog() {
    std::cout << "[KAIZEN GUI] Opening About dialog" << std::endl;

    AdwDialog* about = adw_about_dialog_new();
    adw_about_dialog_set_application_name(ADW_ABOUT_DIALOG(about), "Kaizen Power Tool");
    adw_about_dialog_set_version(ADW_ABOUT_DIALOG(about), "1.0.0");
    adw_about_dialog_set_application_icon(ADW_ABOUT_DIALOG(about), "io.github.kaizen.PowerTool");
    adw_about_dialog_set_developer_name(ADW_ABOUT_DIALOG(about), "Kaizen Open Source Team");
    adw_about_dialog_set_website(ADW_ABOUT_DIALOG(about), "https://github.com/kaizen/kaizen-power-tool");
    adw_about_dialog_set_issue_url(ADW_ABOUT_DIALOG(about), "https://github.com/kaizen/kaizen-power-tool/issues");
    adw_about_dialog_set_license_type(ADW_ABOUT_DIALOG(about), GTK_LICENSE_GPL_3_0);
    adw_about_dialog_set_copyright(ADW_ABOUT_DIALOG(about), "© 2026 Kaizen Contributors");
    adw_about_dialog_set_comments(ADW_ABOUT_DIALOG(about),
        _("Modern Linux hardware and cooling management suite.\n"
          "Complete control over CPU frequencies, GPU power and clock states, and cooling fan curves."));

    const char* developers[] = {"Kaizen Open Source Team", nullptr};
    adw_about_dialog_set_developers(ADW_ABOUT_DIALOG(about), developers);

    adw_dialog_present(about, GTK_WIDGET(m_window));
}

void MainWindow::onAboutClicked(GtkButton*, gpointer userData) {
    MainWindow* self = static_cast<MainWindow*>(userData);
    if (self) {
        self->showAboutDialog();
    }
}

void MainWindow::onElevateClicked(GtkButton*, gpointer userData) {
    MainWindow* self = static_cast<MainWindow*>(userData);
    if (PrivilegeManager::instance().requestAuthorization()) {
        if (self->m_privilegeBanner) {
            adw_banner_set_revealed(self->m_privilegeBanner, FALSE);
        }
        if (self->m_privBadge) {
            gtk_label_set_text(GTK_LABEL(self->m_privBadge), "ROOT");
            gtk_widget_remove_css_class(self->m_privBadge, "privilege-badge-user");
            gtk_widget_add_css_class(self->m_privBadge, "privilege-badge-root");
        }
        if (self->m_elevateBtn) {
            gtk_widget_set_visible(self->m_elevateBtn, FALSE);
        }
        self->showToast(_("Authorization successful. Hardware control enabled."));
    }
}

void MainWindow::onBannerButtonClicked(AdwBanner*, gpointer userData) {
    onElevateClicked(nullptr, userData);
}

void MainWindow::onLanguageChanged(GObject* obj, GParamSpec*, gpointer userData) {
    MainWindow* self = static_cast<MainWindow*>(userData);
    guint sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(obj));
    const auto& langs = i18n::getSupportedLanguages();
    if (sel < langs.size()) {
        std::string chosenLang = langs[sel].code;
        if (chosenLang != i18n::getCurrentLanguage()) {
            i18n::setLanguage(chosenLang);
            self->showToast(_("Language changed. Please restart the application for full effect."));
        }
    }
}

} // namespace fc
