#pragma once

#include <functional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace lupine {
namespace save {

/**
 * A migration upgrades a save's data payload from one schema version to a higher
 * one, editing the JSON in place. Return false to abort the load (e.g. the data
 * is unrecoverable). The function receives only the data payload, not the
 * envelope header.
 */
using SaveMigrationFn = std::function<bool(nlohmann::json& data)>;

struct SaveMigration {
    int fromVersion = 0;
    int toVersion = 0;
    SaveMigrationFn apply;
    std::string description;
};

/**
 * SaveMigrationRegistry - the set of registered schema migrations.
 *
 * When a save is loaded whose recorded schema version is older than the app's
 * current version, the manager calls Migrate to bring the data forward. The
 * registry chains registered steps greedily: at each version it applies the
 * registered migration that advances the version the furthest without overshooting
 * the target. A multi-version jump can therefore be expressed either as a single
 * step (fromVersion 1, toVersion 3) or as a chain (1->2, 2->3).
 */
class SaveMigrationRegistry {
public:
    // Register a migration from `fromVersion` to `toVersion` (toVersion must be
    // greater than fromVersion). A later registration for the same edge replaces
    // the earlier one.
    void Register(int fromVersion, int toVersion, SaveMigrationFn fn,
                  const std::string& description = std::string());

    void Clear();

    // True if there is a chain of registered migrations from `fromVersion` to
    // `toVersion`.
    bool HasPath(int fromVersion, int toVersion) const;

    // Upgrade `data` from `fromVersion` to `targetVersion`. No-op success when the
    // versions are already equal. On failure (missing step or a step returning
    // false), returns false and sets `outError`.
    bool Migrate(nlohmann::json& data, int fromVersion, int targetVersion,
                 std::string& outError) const;

    // Highest toVersion across all registered migrations (0 if none).
    int HighestKnownVersion() const;

    const std::vector<SaveMigration>& GetMigrations() const { return m_Migrations; }
    size_t Count() const { return m_Migrations.size(); }

private:
    // Best registered migration advancing from `fromVersion` without exceeding
    // `targetVersion`, or nullptr if none exists.
    const SaveMigration* FindStep(int fromVersion, int targetVersion) const;

    std::vector<SaveMigration> m_Migrations;
};

} // namespace save
} // namespace lupine
