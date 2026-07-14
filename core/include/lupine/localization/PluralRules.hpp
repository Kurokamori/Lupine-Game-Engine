#pragma once

#include <string>
#include <vector>

namespace lupine {
namespace localization {

/**
 * Plural-category selection following a simplified subset of the Unicode CLDR
 * plural rules. Given a locale and a count, returns one of the CLDR category
 * names: "zero", "one", "two", "few", "many", or "other".
 *
 * A localization entry stores its plural forms keyed by these category names;
 * the manager picks the right form for the active locale and count. Locales
 * without an explicit rule fall back to the common English-style two-form rule
 * (one for n == 1, otherwise other).
 */
class PluralRules {
public:
    // Select the plural category for (locale, count). The locale may be a full
    // BCP-47 tag (e.g. "pt-BR"); only the language subtag is used for matching.
    static std::string Category(const std::string& locale, long count);

    // Ordered list of categories that may be produced for a locale, useful for
    // the editor to present the right set of plural-form fields. Always begins
    // with "other".
    static std::vector<std::string> CategoriesForLocale(const std::string& locale);

private:
    static std::string LanguageSubtag(const std::string& locale);
};

} // namespace localization
} // namespace lupine
