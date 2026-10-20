#include "app/Application.hpp"
#include "core/I18n.hpp"
#include "core/PrivilegeManager.hpp"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    // Check if running as privileged helper daemon
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--helper") {
            std::string sock = (i + 1 < argc) ? argv[i + 1] : "";
            fc::PrivilegeManager::runHelper(sock);
            return 0;
        }
    }

    // Initialize libadwaita
    adw_init();

    // Initialize gettext localization
    fc::i18n::init();

    std::string defaultTab = "summary";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--tab" || arg == "-t") && i + 1 < argc) {
            defaultTab = argv[++i];
        } else if ((arg == "--lang" || arg == "-l") && i + 1 < argc) {
            std::string lang = argv[++i];
            fc::i18n::setLanguage(lang);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << _("Kaizen Power Tool — Hardware & Fan Controller (GTK4 / Libadwaita)") << "\n\n"
                      << _("Usage:") << "\n"
                      << "  kaizengui [options]\n\n"
                      << _("Options:") << "\n"
                      << "  -t, --tab <summary|fans|cpu|gpu|profiles>  " << _("Initial interface view tab") << "\n"
                      << "  -l, --lang <en|ru|kk|ky|tr|uz>             " << _("Interface language (English, Russian, Kazakh, Kyrgyz, Turkish, Uzbek)") << "\n"
                      << "  -h, --help                                 " << _("Show this help message") << "\n";
            return 0;
        }
    }

    fc::Application app(defaultTab);
    return app.run(argc, argv);
}
