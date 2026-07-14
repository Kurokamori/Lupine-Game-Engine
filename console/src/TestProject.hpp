#pragma once

#include <string>

namespace lupine_test {

/**
 * A disposable on-disk project the test suites can scan and mutate.
 *
 * Several engine subsystems (archetypes, the localization manager, scene/project
 * loading) operate on a real project directory rather than in-memory data. This
 * helper stamps out a self-contained project under the system temp directory so
 * those subsystems can be exercised end-to-end without touching the repository.
 *
 * The layout written by Create():
 *   <root>/project.lupine                  - minimal project marker file
 *   <root>/localization.json               - localization config (en, fr)
 *   <root>/localization/ui.loctable        - UI strings + plural forms
 *   <root>/localization/dialogue.csv       - a second table in CSV form
 *   <root>/archetypes/EnemyStats.archetype - base data archetype
 *   <root>/archetypes/BossStats.archetype  - archetype extending EnemyStats
 *
 * The directory is recreated fresh on Create() (any previous contents are
 * removed) so runs are deterministic and order-independent.
 */
class TestProject {
public:
    // Build (or rebuild) the project on disk. Returns true on success.
    static bool Create();

    // Remove the project directory entirely. Safe to call when absent.
    static void Destroy();

    // Absolute path to the project root (valid after Create()).
    static const std::string& Root();

    // Convenience sub-paths.
    static std::string ArchetypesDir();
    static std::string LocalizationDir();

    // Names used by the generated content, exposed so tests stay in sync.
    static const char* kEnemyClass;
    static const char* kBossClass;
    static const char* kUiTable;
    static const char* kDialogueTable;
};

} // namespace lupine_test
