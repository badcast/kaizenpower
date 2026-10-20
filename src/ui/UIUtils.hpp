#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <string>

namespace fc {

class UIUtils {
public:
    static void loadApplicationCss();
    static GtkWidget* createBadge(const std::string& text, const std::string& cssClass);
    static GtkWidget* createMetricCard(const std::string& title, const std::string& initialValue, GtkWidget** outLabel);
    
    static std::string formatTemperature(double tempC);
    static std::string formatFrequency(double freqGhz);
    static std::string formatRpm(int rpm);

    static std::string getLogoPath();
};

} // namespace fc
