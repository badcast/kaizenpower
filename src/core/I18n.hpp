#pragma once

#include <libintl.h>
#include <locale.h>
#include <string>
#include <vector>

#ifndef _
#define _(String) gettext(String)
#endif

#ifndef N_
#define N_(String) (String)
#endif

namespace fc::i18n {

struct LanguageOption {
    std::string code;
    std::string displayName;
};

void init(const std::string& customLocaleDir = "");
void setLanguage(const std::string& langCode);
std::string getCurrentLanguage();
const std::vector<LanguageOption>& getSupportedLanguages();

} // namespace fc::i18n
