#include "lupine/save/SaveMigration.hpp"

namespace lupine {
namespace save {

void SaveMigrationRegistry::Register(int fromVersion, int toVersion, SaveMigrationFn fn,
                                     const std::string& description) {
    if (toVersion <= fromVersion || !fn) {
        return;
    }
    for (SaveMigration& existing : m_Migrations) {
        if (existing.fromVersion == fromVersion && existing.toVersion == toVersion) {
            existing.apply = std::move(fn);
            existing.description = description;
            return;
        }
    }
    SaveMigration migration;
    migration.fromVersion = fromVersion;
    migration.toVersion = toVersion;
    migration.apply = std::move(fn);
    migration.description = description;
    m_Migrations.push_back(std::move(migration));
}

void SaveMigrationRegistry::Clear() {
    m_Migrations.clear();
}

const SaveMigration* SaveMigrationRegistry::FindStep(int fromVersion, int targetVersion) const {
    const SaveMigration* best = nullptr;
    for (const SaveMigration& migration : m_Migrations) {
        if (migration.fromVersion != fromVersion) {
            continue;
        }
        if (migration.toVersion > targetVersion) {
            continue;
        }
        if (!best || migration.toVersion > best->toVersion) {
            best = &migration;
        }
    }
    return best;
}

bool SaveMigrationRegistry::HasPath(int fromVersion, int targetVersion) const {
    int current = fromVersion;
    while (current < targetVersion) {
        const SaveMigration* step = FindStep(current, targetVersion);
        if (!step) {
            return false;
        }
        current = step->toVersion;
    }
    return current == targetVersion;
}

bool SaveMigrationRegistry::Migrate(nlohmann::json& data, int fromVersion, int targetVersion,
                                    std::string& outError) const {
    if (fromVersion == targetVersion) {
        return true;
    }
    if (fromVersion > targetVersion) {
        outError = "save schema version is newer than the application supports";
        return false;
    }

    int current = fromVersion;
    while (current < targetVersion) {
        const SaveMigration* step = FindStep(current, targetVersion);
        if (!step) {
            outError = "no registered migration from schema version " +
                       std::to_string(current) + " toward version " +
                       std::to_string(targetVersion);
            return false;
        }
        if (!step->apply(data)) {
            outError = "migration from schema version " + std::to_string(step->fromVersion) +
                       " to " + std::to_string(step->toVersion) + " failed";
            return false;
        }
        current = step->toVersion;
    }
    return true;
}

int SaveMigrationRegistry::HighestKnownVersion() const {
    int highest = 0;
    for (const SaveMigration& migration : m_Migrations) {
        if (migration.toVersion > highest) {
            highest = migration.toVersion;
        }
    }
    return highest;
}

} // namespace save
} // namespace lupine
