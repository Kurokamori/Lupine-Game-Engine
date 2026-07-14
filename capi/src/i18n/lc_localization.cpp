
#include "i18n/lc_localization.h"
#include "../core/lc_internal.h"

#include <lupine/localization/LocalizationManager.hpp>

#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>

namespace {

void SetLocError(LCResult code, const char* message) {
    ::SetError(code, message);
}

// Copy a std::string into a caller buffer, always null-terminating.
LCResult CopyOut(const std::string& src, char* out, size_t size) {
    if (!out) {
        SetLocError(LC_ERROR_NULL_POINTER, "output buffer is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (size == 0) {
        SetLocError(LC_ERROR_INVALID_PARAMETER, "buffer_size is 0");
        return LC_ERROR_INVALID_PARAMETER;
    }
    CopyStringToBuffer(out, size, src.c_str());
    return LC_SUCCESS;
}

lupine::localization::LocalizationManager& Mgr() {
    return lupine::localization::LocalizationManager::GetInstance();
}

} // anonymous namespace

LC_API LCResult lc_localization_tr(const char* key, char* out_buffer, size_t buffer_size) {
    if (!key) {
        SetLocError(LC_ERROR_NULL_POINTER, "key is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        return CopyOut(Mgr().Translate(key), out_buffer, buffer_size);
    } catch (...) {
        SetLocError(LC_ERROR_INTERNAL_ERROR, "lc_localization_tr failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_localization_tr_in_table(const char* table, const char* key,
                                            char* out_buffer, size_t buffer_size) {
    if (!key || !table) {
        SetLocError(LC_ERROR_NULL_POINTER, "key or table is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        return CopyOut(Mgr().Translate(key, table), out_buffer, buffer_size);
    } catch (...) {
        SetLocError(LC_ERROR_INTERNAL_ERROR, "lc_localization_tr_in_table failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_localization_tr_format(const char* key,
                                          const char* const* arg_names,
                                          const char* const* arg_values,
                                          size_t arg_count,
                                          char* out_buffer, size_t buffer_size) {
    if (!key) {
        SetLocError(LC_ERROR_NULL_POINTER, "key is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (arg_count > 0 && (!arg_names || !arg_values)) {
        SetLocError(LC_ERROR_NULL_POINTER, "arg_names or arg_values is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        std::unordered_map<std::string, std::string> args;
        for (size_t i = 0; i < arg_count; ++i) {
            if (arg_names[i]) {
                args[arg_names[i]] = arg_values[i] ? arg_values[i] : "";
            }
        }
        return CopyOut(Mgr().TranslateFormat(key, args), out_buffer, buffer_size);
    } catch (...) {
        SetLocError(LC_ERROR_INTERNAL_ERROR, "lc_localization_tr_format failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_localization_tr_format_in_table(const char* table, const char* key,
                                                   const char* const* arg_names,
                                                   const char* const* arg_values,
                                                   size_t arg_count,
                                                   char* out_buffer, size_t buffer_size) {
    if (!key || !table) {
        SetLocError(LC_ERROR_NULL_POINTER, "key or table is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (arg_count > 0 && (!arg_names || !arg_values)) {
        SetLocError(LC_ERROR_NULL_POINTER, "arg_names or arg_values is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        std::unordered_map<std::string, std::string> args;
        for (size_t i = 0; i < arg_count; ++i) {
            if (arg_names[i]) {
                args[arg_names[i]] = arg_values[i] ? arg_values[i] : "";
            }
        }
        return CopyOut(Mgr().TranslateFormat(key, args, table), out_buffer, buffer_size);
    } catch (...) {
        SetLocError(LC_ERROR_INTERNAL_ERROR, "lc_localization_tr_format_in_table failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_localization_tr_plural(const char* key, long count,
                                          char* out_buffer, size_t buffer_size) {
    if (!key) {
        SetLocError(LC_ERROR_NULL_POINTER, "key is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        return CopyOut(Mgr().TranslatePlural(key, count), out_buffer, buffer_size);
    } catch (...) {
        SetLocError(LC_ERROR_INTERNAL_ERROR, "lc_localization_tr_plural failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_localization_tr_plural_full(const char* table, const char* key, long count,
                                               const char* const* arg_names,
                                               const char* const* arg_values,
                                               size_t arg_count,
                                               char* out_buffer, size_t buffer_size) {
    if (!key || !table) {
        SetLocError(LC_ERROR_NULL_POINTER, "key or table is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (arg_count > 0 && (!arg_names || !arg_values)) {
        SetLocError(LC_ERROR_NULL_POINTER, "arg_names or arg_values is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        std::unordered_map<std::string, std::string> args;
        for (size_t i = 0; i < arg_count; ++i) {
            if (arg_names[i]) {
                args[arg_names[i]] = arg_values[i] ? arg_values[i] : "";
            }
        }
        return CopyOut(Mgr().TranslatePlural(key, count, args, table), out_buffer, buffer_size);
    } catch (...) {
        SetLocError(LC_ERROR_INTERNAL_ERROR, "lc_localization_tr_plural_full failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_localization_has_key(const char* key, int* out_has) {
    if (!key || !out_has) {
        SetLocError(LC_ERROR_NULL_POINTER, "key or out_has is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_has = Mgr().HasKey(key) ? 1 : 0;
        return LC_SUCCESS;
    } catch (...) {
        SetLocError(LC_ERROR_INTERNAL_ERROR, "lc_localization_has_key failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_localization_set_locale(const char* locale) {
    if (!locale) {
        SetLocError(LC_ERROR_NULL_POINTER, "locale is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        Mgr().SetLocale(locale);
        return LC_SUCCESS;
    } catch (...) {
        SetLocError(LC_ERROR_INTERNAL_ERROR, "lc_localization_set_locale failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_localization_get_locale(char* out_buffer, size_t buffer_size) {
    try {
        return CopyOut(Mgr().GetLocale(), out_buffer, buffer_size);
    } catch (...) {
        SetLocError(LC_ERROR_INTERNAL_ERROR, "lc_localization_get_locale failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_localization_get_fallback_locale(char* out_buffer, size_t buffer_size) {
    try {
        return CopyOut(Mgr().GetFallbackLocale(), out_buffer, buffer_size);
    } catch (...) {
        SetLocError(LC_ERROR_INTERNAL_ERROR, "lc_localization_get_fallback_locale failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_localization_get_locale_count(int* out_count) {
    if (!out_count) {
        SetLocError(LC_ERROR_NULL_POINTER, "out_count is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_count = static_cast<int>(Mgr().GetAvailableLocales().size());
        return LC_SUCCESS;
    } catch (...) {
        SetLocError(LC_ERROR_INTERNAL_ERROR, "lc_localization_get_locale_count failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_localization_get_locale_at(int index, char* out_buffer, size_t buffer_size) {
    try {
        std::vector<std::string> locales = Mgr().GetAvailableLocales();
        if (index < 0 || index >= static_cast<int>(locales.size())) {
            SetLocError(LC_ERROR_INVALID_PARAMETER, "locale index out of range");
            return LC_ERROR_INVALID_PARAMETER;
        }
        return CopyOut(locales[static_cast<size_t>(index)], out_buffer, buffer_size);
    } catch (...) {
        SetLocError(LC_ERROR_INTERNAL_ERROR, "lc_localization_get_locale_at failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_localization_reload(void) {
    try {
        Mgr().ReloadTables();
        return LC_SUCCESS;
    } catch (...) {
        SetLocError(LC_ERROR_INTERNAL_ERROR, "lc_localization_reload failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_localization_set_pseudolocalization(int enabled) {
    try {
        Mgr().SetPseudolocalizationEnabled(enabled != 0);
        return LC_SUCCESS;
    } catch (...) {
        SetLocError(LC_ERROR_INTERNAL_ERROR, "lc_localization_set_pseudolocalization failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_localization_is_pseudolocalization(int* out_enabled) {
    if (!out_enabled) {
        SetLocError(LC_ERROR_NULL_POINTER, "out_enabled is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_enabled = Mgr().IsPseudolocalizationEnabled() ? 1 : 0;
        return LC_SUCCESS;
    } catch (...) {
        SetLocError(LC_ERROR_INTERNAL_ERROR, "lc_localization_is_pseudolocalization failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_localization_is_enabled(int* out_enabled) {
    if (!out_enabled) {
        SetLocError(LC_ERROR_NULL_POINTER, "out_enabled is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_enabled = Mgr().IsEnabled() ? 1 : 0;
        return LC_SUCCESS;
    } catch (...) {
        SetLocError(LC_ERROR_INTERNAL_ERROR, "lc_localization_is_enabled failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
