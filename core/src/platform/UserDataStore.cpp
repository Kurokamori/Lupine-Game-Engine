#include "lupine/platform/UserDataStore.hpp"

#if defined(__EMSCRIPTEN__) || defined(LUPINE_PLATFORM_WEB)
    #include <emscripten.h>
#endif

namespace lupine {
namespace platform {

UserDataStore& UserDataStore::GetInstance() {
    static UserDataStore instance;
    return instance;
}

void UserDataStore::MarkDirty() {
    m_Dirty.store(true, std::memory_order_relaxed);
}

bool UserDataStore::IsDirty() const {
    return m_Dirty.load(std::memory_order_relaxed);
}

void UserDataStore::Flush() {
    // Clear the flag before starting the write. A write that lands *during* the flush
    // re-sets it, so it is picked up by the next Flush rather than being swallowed.
    if (!m_Dirty.exchange(false, std::memory_order_relaxed)) {
        return;
    }

#if defined(__EMSCRIPTEN__) || defined(LUPINE_PLATFORM_WEB)
    // FS.syncfs walks the mount and reconciles it against IndexedDB. Two of them running
    // at once can interleave and leave the store inconsistent, so a sync already in
    // flight sets a pending bit instead of starting a second one; the in-flight callback
    // then runs exactly one more pass, which subsumes every write that arrived meanwhile.
    EM_ASM({
        if (typeof FS === 'undefined') {
            return;
        }

        if (Module.__lupineUserSyncInFlight) {
            Module.__lupineUserSyncPending = true;
            return;
        }

        var runSync = function() {
            Module.__lupineUserSyncInFlight = true;
            FS.syncfs(false, function(err) {
                Module.__lupineUserSyncInFlight = false;
                if (err) {
                    console.error('[Lupine] failed to persist user:// to IndexedDB:', err);
                }
                if (Module.__lupineUserSyncPending) {
                    Module.__lupineUserSyncPending = false;
                    runSync();
                }
            });
        };

        runSync();
    });
#endif
}

} // namespace platform
} // namespace lupine
