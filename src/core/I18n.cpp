#include "I18n.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace fc::i18n {

static std::string s_activeLanguage = "en";
static std::string s_localeDir = "";

static const std::vector<LanguageOption> s_languages = {
    {"en", "English"},
    {"ru", "Русский (Russian)"},
    {"kk", "Қазақша (Kazakh)"},
    {"ky", "Кыргызча (Kyrgyz)"},
    {"tr", "Türkçe (Turkish)"},
    {"uz", "O'zbekcha (Uzbek)"}
};

const std::vector<LanguageOption>& getSupportedLanguages() {
    return s_languages;
}

std::string getCurrentLanguage() {
    return s_activeLanguage;
}

void init(const std::string& customLocaleDir) {
    setlocale(LC_ALL, "");

    // Detect locale dir
    if (!customLocaleDir.empty() && fs::exists(customLocaleDir)) {
        s_localeDir = customLocaleDir;
    } else if (fs::exists("locale")) {
        s_localeDir = fs::canonical("locale").string();
    } else if (fs::exists("../locale")) {
        s_localeDir = fs::canonical("../locale").string();
    } else if (fs::exists("build/locale")) {
        s_localeDir = fs::canonical("build/locale").string();
    } else if (fs::exists("/usr/share/locale")) {
        s_localeDir = "/usr/share/locale";
    } else {
        s_localeDir = "./locale";
    }

    bindtextdomain("fancontroller", s_localeDir.c_str());
    bind_textdomain_codeset("fancontroller", "UTF-8");
    textdomain("fancontroller");

    const char* envLang = getenv("LANGUAGE");
    if (!envLang || std::string(envLang).empty()) {
        envLang = getenv("LANG");
    }
    if (envLang) {
        std::string el(envLang);
        if (el.find("ru") == 0) s_activeLanguage = "ru";
        else if (el.find("kk") == 0) s_activeLanguage = "kk";
        else if (el.find("ky") == 0) s_activeLanguage = "ky";
        else if (el.find("tr") == 0) s_activeLanguage = "tr";
        else if (el.find("uz") == 0) s_activeLanguage = "uz";
        else s_activeLanguage = "en";
    }
}

void setLanguage(const std::string& langCode) {
    s_activeLanguage = langCode;
    setenv("LANGUAGE", langCode.c_str(), 1);
    setenv("LC_ALL", (langCode + ".UTF-8").c_str(), 1);
    setenv("LC_MESSAGES", (langCode + ".UTF-8").c_str(), 1);
    setlocale(LC_ALL, "");
    setlocale(LC_MESSAGES, "");

    if (!s_localeDir.empty()) {
        bindtextdomain("fancontroller", s_localeDir.c_str());
        bind_textdomain_codeset("fancontroller", "UTF-8");
        textdomain("fancontroller");
    }
}

} // namespace fc::i18n
