#pragma once

#include <adwaita.h>
#include <memory>
#include "MainWindow.hpp"

namespace fc {

class Application {
public:
    Application(const std::string& defaultTab = "fans");
    ~Application();

    int run(int argc, char* argv[]);

private:
    AdwApplication* m_app = nullptr;
    std::string m_defaultTab;
    std::unique_ptr<MainWindow> m_mainWindow;

    static void onActivate(GApplication* gApp, gpointer userData);
    static void onStartup(GApplication* gApp, gpointer userData);
};

} // namespace fc
