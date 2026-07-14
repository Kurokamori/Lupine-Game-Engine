#include "lupine/localization/PluralRules.hpp"
#include <algorithm>
#include <cstdlib>

namespace lupine {
namespace localization {

std::string PluralRules::LanguageSubtag(const std::string& locale) {
    std::string lang = locale;
    size_t sep = lang.find_first_of("-_");
    if (sep != std::string::npos) {
        lang = lang.substr(0, sep);
    }
    std::transform(lang.begin(), lang.end(), lang.begin(), ::tolower);
    return lang;
}

std::string PluralRules::Category(const std::string& locale, long count) {
    const std::string lang = LanguageSubtag(locale);
    const long n = std::labs(count);
    const long mod10 = n % 10;
    const long mod100 = n % 100;

    // Languages with no plural distinction (one form for everything).
    if (lang == "ja" || lang == "zh" || lang == "ko" || lang == "vi" ||
        lang == "th" || lang == "id" || lang == "ms" || lang == "lo" ||
        lang == "km" || lang == "my") {
        return "other";
    }

    // French / Brazilian-Portuguese style: 0 and 1 are "one".
    if (lang == "fr" || lang == "pt" || lang == "hy" || lang == "ak" ||
        lang == "ln" || lang == "wa" || lang == "hi") {
        return (n == 0 || n == 1) ? "one" : "other";
    }

    // Russian / Ukrainian / Serbian / Croatian family (East-Slavic style).
    if (lang == "ru" || lang == "uk" || lang == "be" || lang == "sr" ||
        lang == "hr" || lang == "bs") {
        if (mod10 == 1 && mod100 != 11) {
            return "one";
        }
        if (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14)) {
            return "few";
        }
        return "many";
    }

    // Polish.
    if (lang == "pl") {
        if (n == 1) {
            return "one";
        }
        if (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14)) {
            return "few";
        }
        return "many";
    }

    // Czech / Slovak.
    if (lang == "cs" || lang == "sk") {
        if (n == 1) {
            return "one";
        }
        if (n >= 2 && n <= 4) {
            return "few";
        }
        return "other";
    }

    // Arabic (six categories).
    if (lang == "ar") {
        if (n == 0) {
            return "zero";
        }
        if (n == 1) {
            return "one";
        }
        if (n == 2) {
            return "two";
        }
        if (mod100 >= 3 && mod100 <= 10) {
            return "few";
        }
        if (mod100 >= 11 && mod100 <= 99) {
            return "many";
        }
        return "other";
    }

    // Welsh.
    if (lang == "cy") {
        if (n == 0) {
            return "zero";
        }
        if (n == 1) {
            return "one";
        }
        if (n == 2) {
            return "two";
        }
        if (n == 3) {
            return "few";
        }
        if (n == 6) {
            return "many";
        }
        return "other";
    }

    // Lithuanian.
    if (lang == "lt") {
        if (mod10 == 1 && (mod100 < 11 || mod100 > 19)) {
            return "one";
        }
        if (mod10 >= 2 && mod10 <= 9 && (mod100 < 11 || mod100 > 19)) {
            return "few";
        }
        return "other";
    }

    // Default: English / German / Spanish / Italian / Dutch / Scandinavian etc.
    return (n == 1) ? "one" : "other";
}

std::vector<std::string> PluralRules::CategoriesForLocale(const std::string& locale) {
    const std::string lang = LanguageSubtag(locale);

    if (lang == "ja" || lang == "zh" || lang == "ko" || lang == "vi" ||
        lang == "th" || lang == "id" || lang == "ms" || lang == "lo" ||
        lang == "km" || lang == "my") {
        return {"other"};
    }
    if (lang == "ar") {
        return {"zero", "one", "two", "few", "many", "other"};
    }
    if (lang == "cy") {
        return {"zero", "one", "two", "few", "many", "other"};
    }
    if (lang == "ru" || lang == "uk" || lang == "be" || lang == "sr" ||
        lang == "hr" || lang == "bs" || lang == "pl") {
        return {"one", "few", "many", "other"};
    }
    if (lang == "cs" || lang == "sk" || lang == "lt") {
        return {"one", "few", "other"};
    }

    return {"one", "other"};
}

} // namespace localization
} // namespace lupine
