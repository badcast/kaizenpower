#include "UIUtils.hpp"
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fc {

static const char* fallbackCss = R"CSS(
.fan-card {
    background-color: alpha(@card_bg_color, 0.9);
    border-radius: 8px;
    padding: 6px 10px;
    margin-bottom: 5px;
    border: 1px solid alpha(@borders, 0.4);
}

.fan-badge {
    font-size: 9.5px;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.2px;
    padding: 1px 6px;
    border-radius: 6px;
}

.fan-badge-cpu {
    background-color: alpha(#1c71d8, 0.18);
    color: #3584e4;
    border: 1px solid alpha(#3584e4, 0.35);
}

.fan-badge-gpu {
    background-color: alpha(#9141ac, 0.18);
    color: #c061cb;
    border: 1px solid alpha(#c061cb, 0.35);
}

.fan-badge-case {
    background-color: alpha(#26a269, 0.18);
    color: #33d17a;
    border: 1px solid alpha(#33d17a, 0.35);
}

.fan-badge-unknown {
    background-color: alpha(#5e5c64, 0.18);
    color: #9a9996;
    border: 1px solid alpha(#9a9996, 0.35);
}

.rpm-value {
    font-size: 15px;
    font-weight: 700;
}

.metric-box {
    background-color: alpha(@card_bg_color, 0.65);
    border-radius: 6px;
    padding: 5px 8px;
    border: 1px solid alpha(@borders, 0.4);
}

.metric-title {
    font-size: 10.5px;
    font-weight: 600;
    opacity: 0.65;
    margin-bottom: 1px;
}

.compact-title {
    font-size: 12.5px;
    font-weight: 700;
}

.metric-number {
    font-size: 14px;
    font-weight: 700;
}

.profile-card {
    border-radius: 8px;
    padding: 6px 10px;
    margin-bottom: 5px;
    background-color: alpha(@card_bg_color, 0.75);
    border: 1px solid alpha(@borders, 0.4);
}

.profile-card:hover {
    background-color: alpha(@card_bg_color, 0.95);
}

.profile-card.active-profile {
    border: 1.5px solid #3584e4;
    background-color: alpha(#3584e4, 0.07);
}

.pill-btn {
    border-radius: 10px;
    font-size: 10.5px;
    font-weight: 600;
    padding: 2px 8px;
    min-height: 22px;
    transition: all 150ms ease-in-out;
}

.pill-btn.active, .pill-btn.suggested-action {
    background-color: #3584e4;
    color: #ffffff;
    font-weight: 700;
    box-shadow: 0 1px 3px rgba(0, 0, 0, 0.25);
}

.curve-box {
    background-color: alpha(black, 0.12);
    border-radius: 6px;
    padding: 6px;
    border: 1px solid alpha(@borders, 0.3);
}
)CSS";

void UIUtils::loadApplicationCss() {
    GtkCssProvider* provider = gtk_css_provider_new();

    // Check if style.css exists in current directory or resources
    std::string cssPath = "resources/style.css";
    if (!std::filesystem::exists(cssPath)) {
        cssPath = "../resources/style.css";
    }

    if (std::filesystem::exists(cssPath)) {
        gtk_css_provider_load_from_path(provider, cssPath.c_str());
    } else {
        gtk_css_provider_load_from_string(provider, fallbackCss);
    }

    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);
}

GtkWidget* UIUtils::createBadge(const std::string& text, const std::string& cssClass) {
    GtkWidget* label = gtk_label_new(text.c_str());
    gtk_widget_add_css_class(label, "fan-badge");
    if (!cssClass.empty()) {
        gtk_widget_add_css_class(label, cssClass.c_str());
    }
    return label;
}

GtkWidget* UIUtils::createMetricCard(const std::string& title, const std::string& initialValue, GtkWidget** outLabel) {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(box, "metric-box");
    gtk_widget_set_hexpand(box, TRUE);

    GtkWidget* titleLabel = gtk_label_new(title.c_str());
    gtk_widget_add_css_class(titleLabel, "metric-title");
    gtk_widget_set_halign(titleLabel, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), titleLabel);

    GtkWidget* valLabel = gtk_label_new(initialValue.c_str());
    gtk_widget_add_css_class(valLabel, "metric-number");
    gtk_widget_set_halign(valLabel, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), valLabel);

    if (outLabel) {
        *outLabel = valLabel;
    }

    return box;
}

std::string UIUtils::formatTemperature(double tempC) {
    if (tempC <= 0.0) return "N/A";
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << tempC << " °C";
    return ss.str();
}

std::string UIUtils::formatFrequency(double freqGhz) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << freqGhz << " GHz";
    return ss.str();
}

std::string UIUtils::formatRpm(int rpm) {
    return std::to_string(rpm) + " RPM";
}

std::string UIUtils::getLogoPath() {
    const std::vector<std::string> candidates = {
        "resources/logo.png",
        "../resources/logo.png",
        "/usr/share/kaizen/resources/logo.png",
        "/usr/share/pixmaps/io.github.kaizen.PowerTool.png"
    };
    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
    return "";
}

} // namespace fc
