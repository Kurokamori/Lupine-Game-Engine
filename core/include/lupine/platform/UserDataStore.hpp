#pragma once

#include <atomic>

namespace lupine {
namespace platform {

/**
 * Persistence for the writable user:// space.
 *
 * On native platforms user:// is a real directory and writes are already durable, so
 * this class is inert - MarkDirty/Flush compile down to a flag set and an early return.
 *
 * On Emscripten the picture is different. Emscripten's default filesystem (MEMFS) lives
 * in RAM and is discarded when the page unloads, so a save written through it survives
 * only until reload. Durable storage means IDBFS, which is backed by IndexedDB, and
 * IndexedDB is asynchronous - it cannot be read or written from a synchronous C++ call.
 *
 * The split that makes this work:
 *
 *  - LOAD happens once, in JS, before main(). runtime/web/shell.html mounts IDBFS at
 *    kWebUserDataPath and calls FS.syncfs(true, ...) inside a run dependency, so the
 *    runtime does not start until IndexedDB has been read into the in-memory image.
 *    By the time any C++ code opens a user:// file, the data is already there.
 *
 *  - SAVE happens here. Writes go to the in-memory image synchronously (so the calling
 *    code sees normal POSIX semantics), and Flush() pushes that image back to IndexedDB
 *    via FS.syncfs(false, ...), which completes asynchronously in the background. The
 *    game never blocks on it.
 *
 * Flush() is cheap when nothing changed, so the frame loop can call it unconditionally.
 * Overlapping syncfs calls are coalesced (see the in-flight guard in the implementation)
 * because concurrent syncfs operations can interleave and corrupt the store.
 */
class UserDataStore {
public:
    /**
     * The user:// mount point on web. runtime/web/shell.html mounts IDBFS at exactly
     * this path; VirtualFileSystem::GetDefaultUserDataPath returns it on Emscripten.
     * Changing one without the other silently drops persistence.
     */
    static constexpr const char* kWebUserDataPath = "/lupine_user";

    static UserDataStore& GetInstance();

    /** Record that user:// changed and needs to be written back. Cheap; safe to spam. */
    void MarkDirty();

    /** True when there are changes not yet pushed to durable storage. */
    bool IsDirty() const;

    /**
     * Push pending user:// changes to durable storage. No-op when nothing is dirty.
     * On web this starts an asynchronous IndexedDB write and returns immediately - it
     * does not wait for completion, and it is safe to call every frame.
     */
    void Flush();

private:
    UserDataStore() = default;
    ~UserDataStore() = default;
    UserDataStore(const UserDataStore&) = delete;
    UserDataStore& operator=(const UserDataStore&) = delete;

    std::atomic<bool> m_Dirty{false};
};

} // namespace platform
} // namespace lupine
