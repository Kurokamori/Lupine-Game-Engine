/**
 * @file lc_localization.h
 * @brief Lupine Engine C API - Localization / translation
 *
 * Look up translation keys for the active locale, switch locales, and query the
 * available locales. Translation resolves the active locale, then the configured
 * fallback, then the default locale, and finally returns the key itself if
 * nothing matches.
 *
 * String results are written into a caller-provided buffer and always
 * null-terminated (truncated to buffer_size - 1 if necessary).
 */

#ifndef LUPINE_CAPI_LOCALIZATION_H
#define LUPINE_CAPI_LOCALIZATION_H

#include "core/lc_core.h"
#include <stddef.h>

/* ============================================================================
 * Translation
 * ============================================================================ */

/**
 * @brief Translate a key for the active locale (searches all tables).
 * @param key Translation key.
 * @param out_buffer Destination buffer for the translated string.
 * @param buffer_size Size of out_buffer in bytes.
 * @return LC_SUCCESS on success.
 */
LC_API LCResult lc_localization_tr(const char* key, char* out_buffer, size_t buffer_size);

/**
 * @brief Translate a key constrained to a single named table.
 */
LC_API LCResult lc_localization_tr_in_table(const char* table, const char* key,
                                            char* out_buffer, size_t buffer_size);

/**
 * @brief Translate a key, then substitute {name} placeholders.
 * @param key Translation key.
 * @param arg_names Array of placeholder names (length arg_count).
 * @param arg_values Array of replacement values (length arg_count).
 * @param arg_count Number of name/value pairs.
 * @param out_buffer Destination buffer.
 * @param buffer_size Size of out_buffer in bytes.
 */
LC_API LCResult lc_localization_tr_format(const char* key,
                                          const char* const* arg_names,
                                          const char* const* arg_values,
                                          size_t arg_count,
                                          char* out_buffer, size_t buffer_size);

/**
 * @brief Translate a key constrained to a single named table, then substitute
 *        {name} placeholders.
 * @param table Name of the table to search.
 * @param key Translation key.
 * @param arg_names Array of placeholder names (length arg_count).
 * @param arg_values Array of replacement values (length arg_count).
 * @param arg_count Number of name/value pairs.
 * @param out_buffer Destination buffer.
 * @param buffer_size Size of out_buffer in bytes.
 */
LC_API LCResult lc_localization_tr_format_in_table(const char* table, const char* key,
                                                   const char* const* arg_names,
                                                   const char* const* arg_values,
                                                   size_t arg_count,
                                                   char* out_buffer, size_t buffer_size);

/**
 * @brief Plural-aware translation. Selects the plural form for the active locale
 *        and count, then substitutes {count}.
 */
LC_API LCResult lc_localization_tr_plural(const char* key, long count,
                                          char* out_buffer, size_t buffer_size);

/**
 * @brief Plural-aware translation constrained to a single named table, with
 *        explicit {name} placeholder substitution. Selects the plural form for
 *        the active locale and count, substituting {count} plus the supplied args.
 * @param table Name of the table to search.
 * @param key Translation key.
 * @param count Quantity selecting the plural form.
 * @param arg_names Array of placeholder names (length arg_count).
 * @param arg_values Array of replacement values (length arg_count).
 * @param arg_count Number of name/value pairs.
 * @param out_buffer Destination buffer.
 * @param buffer_size Size of out_buffer in bytes.
 */
LC_API LCResult lc_localization_tr_plural_full(const char* table, const char* key, long count,
                                               const char* const* arg_names,
                                               const char* const* arg_values,
                                               size_t arg_count,
                                               char* out_buffer, size_t buffer_size);

/**
 * @brief Test whether a key exists in any loaded table.
 * @param key Translation key.
 * @param out_has Receives 1 if the key exists, 0 otherwise.
 */
LC_API LCResult lc_localization_has_key(const char* key, int* out_has);

/* ============================================================================
 * Locale management
 * ============================================================================ */

/**
 * @brief Set the active locale (persisted to user://locale.cfg).
 */
LC_API LCResult lc_localization_set_locale(const char* locale);

/**
 * @brief Get the active locale.
 */
LC_API LCResult lc_localization_get_locale(char* out_buffer, size_t buffer_size);

/**
 * @brief Get the configured fallback locale.
 */
LC_API LCResult lc_localization_get_fallback_locale(char* out_buffer, size_t buffer_size);

/**
 * @brief Get the number of available locales (config locales + table locales).
 * @param out_count Receives the locale count.
 */
LC_API LCResult lc_localization_get_locale_count(int* out_count);

/**
 * @brief Get the available locale at the given index.
 */
LC_API LCResult lc_localization_get_locale_at(int index, char* out_buffer, size_t buffer_size);

/* ============================================================================
 * Maintenance
 * ============================================================================ */

/**
 * @brief Reload table files from disk (dev convenience).
 */
LC_API LCResult lc_localization_reload(void);

/**
 * @brief Enable or disable pseudolocalization (layout testing).
 * @param enabled Non-zero to enable.
 */
LC_API LCResult lc_localization_set_pseudolocalization(int enabled);

/**
 * @brief Query pseudolocalization state.
 * @param out_enabled Receives 1 if enabled, 0 otherwise.
 */
LC_API LCResult lc_localization_is_pseudolocalization(int* out_enabled);

/**
 * @brief Query whether localization is enabled for the project.
 * @param out_enabled Receives 1 if enabled, 0 otherwise.
 */
LC_API LCResult lc_localization_is_enabled(int* out_enabled);

#endif /* LUPINE_CAPI_LOCALIZATION_H */
