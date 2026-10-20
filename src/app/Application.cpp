#include "Application.hpp"
#include "../ui/UIUtils.hpp"

namespace fc {

Application::Application(const std::string& defaultTab)
    : m_defaultTab(defaultTab) {
    m_app = adw_application_new("io.github.kaizen.PowerTool", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(m_app, "startup", G_CALLBACK(onStartup), this);
    g_signal_connect(m_app, "activate", G_CALLBACK(onActivate), this);
}

Application::~Application() {
    if (m_app) {
        g_object_unref(m_app);
        m_app = nullptr;
    }
}

void Application::onStartup(GApplication*, gpointer) {
    UIUtils::loadApplicationCss();

    GdkDisplay* display = gdk_display_get_default();
    if (display) {
        GtkIconTheme* theme = gtk_icon_theme_get_for_display(display);
        if (theme) {
            gtk_icon_theme_add_search_path(theme, "resources/icons");
            gtk_icon_theme_add_search_path(theme, "../resources/icons");
            gtk_icon_theme_add_search_path(theme, "/usr/share/kaizen/resources/icons");
            gtk_icon_theme_add_search_path(theme, "/usr/share/icons");
        }
    }
    gtk_window_set_default_icon_name("io.github.kaizen.PowerTool");
}

void Application::onActivate(GApplication* gApp, gpointer userData) {
    Application* self = static_cast<Application*>(userData);
    if (!self->m_mainWindow) {
        self->m_mainWindow = std::make_unique<MainWindow>(ADW_APPLICATION(gApp), self->m_defaultTab);
    }
    self->m_mainWindow->show();
}

int Application::run(int, char* argv[]) {
    // Pass only program name to GApplication to avoid conflicts with custom CLI arguments
    int dummyArgc = 1;
    return g_application_run(G_APPLICATION(m_app), dummyArgc, argv);
}

} // namespace fc
