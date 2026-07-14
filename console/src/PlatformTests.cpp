#include "lupine/logger/Logger.hpp"
#include "lupine/platform/Platform.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <sstream>

using namespace lupine::platform;

void WaitForInput();

void RunPlatformTests() {
    std::cout << "=== Lupine Engine Platform Module Test ===" << std::endl;
    std::cout << std::endl;

    // Initialize logger
    lupine::Logger::Init("lupine_platform.log", true);

    LOG_CORE_INFO("=== Starting Platform Module Tests ===");

    // ========================================================================
    // Test 1: Platform Detection and Info
    // ========================================================================
    LOG_CORE_INFO("=== Test 1: Platform Detection ===");
    std::cout << "\n--- Platform Detection ---" << std::endl;
    std::cout << Platform::GetPlatformInfo() << std::endl;

    LOG_CORE_INFO("Platform: {}", Platform::GetPlatformName());
    LOG_CORE_INFO("Architecture: {}", Platform::GetArchitectureName());
    LOG_CORE_INFO("Compiler: {}", Platform::GetCompilerName());
    LOG_CORE_INFO("Build: {}", Platform::GetBuildConfiguration());
    LOG_CORE_INFO("Is Windows: {}", Platform::IsWindows());
    LOG_CORE_INFO("Is Linux: {}", Platform::IsLinux());
    LOG_CORE_INFO("Is macOS: {}", Platform::IsMacOS());
    LOG_CORE_INFO("Is Debug: {}", Platform::IsDebug());

    WaitForInput();

    // ========================================================================
    // Test 2: Timing Utilities
    // ========================================================================
    LOG_CORE_INFO("=== Test 2: Timing Utilities ===");
    std::cout << "\n--- Timing Utilities ---" << std::endl;

    // Test basic timing
    auto start = Timing::Now();
    Timing::SleepMilliseconds(100);
    auto end = Timing::Now();
    double elapsed = Timing::GetElapsedMilliseconds(start, end);
    std::cout << "Sleep 100ms took: " << elapsed << " ms" << std::endl;
    LOG_CORE_INFO("Sleep 100ms took: {:.2f} ms", elapsed);

    // Test ScopedTimer
    std::cout << "\nTesting ScopedTimer (watch logs for output)..." << std::endl;
    {
        ScopedTimer timer("Test Operation");
        Timing::SleepMilliseconds(50);
    }

    // Test Stopwatch
    std::cout << "\nTesting Stopwatch..." << std::endl;
    Stopwatch sw;
    sw.Start();
    Timing::SleepMilliseconds(30);
    sw.Stop();
    std::cout << "Stopwatch elapsed: " << sw.GetElapsedMilliseconds() << " ms" << std::endl;
    LOG_CORE_INFO("Stopwatch test: {:.2f} ms", sw.GetElapsedMilliseconds());

    sw.Start(); // Resume
    Timing::SleepMilliseconds(20);
    sw.Stop();
    std::cout << "Stopwatch total elapsed: " << sw.GetElapsedMilliseconds() << " ms" << std::endl;
    LOG_CORE_INFO("Stopwatch total: {:.2f} ms", sw.GetElapsedMilliseconds());

    WaitForInput();

    // ========================================================================
    // Test 3: Path Utilities
    // ========================================================================
    LOG_CORE_INFO("=== Test 3: Path Utilities ===");
    std::cout << "\n--- Path Utilities ---" << std::endl;

    std::string path1 = "foo/bar";
    std::string path2 = "baz/file.txt";
    std::string joined = Path::Join(path1, path2);
    std::cout << "Join(\"" << path1 << "\", \"" << path2 << "\") = \"" << joined << "\"" << std::endl;
    LOG_CORE_INFO("Path join test: {}", joined);

    std::string messy = "foo//bar/../baz/./file.txt";
    std::string normalized = Path::Normalize(messy);
    std::cout << "Normalize(\"" << messy << "\") = \"" << normalized << "\"" << std::endl;
    LOG_CORE_INFO("Path normalize test: {}", normalized);

    std::string fullPath = "foo/bar/file.txt";
    std::cout << "GetDirectory(\"" << fullPath << "\") = \"" << Path::GetDirectory(fullPath) << "\"" << std::endl;
    std::cout << "GetFilename(\"" << fullPath << "\") = \"" << Path::GetFilename(fullPath) << "\"" << std::endl;
    std::cout << "GetFilenameWithoutExtension(\"" << fullPath << "\") = \"" << Path::GetFilenameWithoutExtension(fullPath) << "\"" << std::endl;
    std::cout << "GetExtension(\"" << fullPath << "\") = \"" << Path::GetExtension(fullPath) << "\"" << std::endl;

    LOG_CORE_INFO("Path utilities tested successfully");

    WaitForInput();

    // ========================================================================
    // Test 4: File System Operations
    // ========================================================================
    LOG_CORE_INFO("=== Test 4: File System Operations ===");
    std::cout << "\n--- File System Operations ---" << std::endl;

    // Create test directory
    std::string testDir = "lupine_test_dir";
    std::cout << "Creating test directory: " << testDir << std::endl;
    auto createResult = FileSystem::CreateDirectory(testDir, true);
    if (createResult.success) {
        std::cout << "  Success!" << std::endl;
        LOG_CORE_INFO("Created test directory: {}", testDir);
    } else {
        std::cout << "  Failed: " << createResult.error << std::endl;
        LOG_CORE_ERROR("Failed to create test directory: {}", createResult.error);
    }

    // Write test file
    std::string testFile = Path::Join(testDir, "test_file.txt");
    std::string testContent = "Hello from Lupine Engine!\nThis is a test file.\n";
    std::cout << "\nWriting test file: " << testFile << std::endl;
    auto writeResult = FileSystem::WriteFile(testFile, testContent);
    if (writeResult.success) {
        std::cout << "  Success!" << std::endl;
        LOG_CORE_INFO("Wrote test file: {}", testFile);
    } else {
        std::cout << "  Failed: " << writeResult.error << std::endl;
        LOG_CORE_ERROR("Failed to write test file: {}", writeResult.error);
    }

    // Read test file
    std::cout << "\nReading test file: " << testFile << std::endl;
    auto readResult = FileSystem::ReadFile(testFile);
    if (readResult.success) {
        std::cout << "  Success! Content:" << std::endl;
        std::cout << "  ---" << std::endl;
        std::cout << "  " << readResult.data;
        std::cout << "  ---" << std::endl;
        LOG_CORE_INFO("Read test file successfully");
    } else {
        std::cout << "  Failed: " << readResult.error << std::endl;
        LOG_CORE_ERROR("Failed to read test file: {}", readResult.error);
    }

    // Test file metadata
    std::cout << "\nGetting file metadata..." << std::endl;
    auto metaResult = FileSystem::GetMetadata(testFile);
    if (metaResult.success) {
        std::cout << "  Size: " << metaResult.data.size << " bytes" << std::endl;
        std::cout << "  Is File: " << (metaResult.data.isFile ? "Yes" : "No") << std::endl;
        std::cout << "  Is Directory: " << (metaResult.data.isDirectory ? "Yes" : "No") << std::endl;
        LOG_CORE_INFO("File size: {} bytes", metaResult.data.size);
    }

    // Test file operations
    std::string testFile2 = Path::Join(testDir, "test_file_copy.txt");
    std::cout << "\nCopying file to: " << testFile2 << std::endl;
    auto copyResult = FileSystem::CopyFile(testFile, testFile2, true);
    if (copyResult.success) {
        std::cout << "  Success!" << std::endl;
        LOG_CORE_INFO("Copied file successfully");
    } else {
        std::cout << "  Failed: " << copyResult.error << std::endl;
    }

    // List directory
    std::cout << "\nListing directory contents..." << std::endl;
    auto listResult = FileSystem::ListDirectory(testDir, false);
    if (listResult.success) {
        std::cout << "  Found " << listResult.data.size() << " items:" << std::endl;
        for (const auto& item : listResult.data) {
            std::cout << "    - " << Path::GetFilename(item) << std::endl;
        }
        LOG_CORE_INFO("Listed {} items in directory", listResult.data.size());
    }

    // Cleanup
    std::cout << "\nCleaning up test files..." << std::endl;
    FileSystem::DeleteFile(testFile);
    FileSystem::DeleteFile(testFile2);
    FileSystem::DeleteDirectory(testDir, true);
    std::cout << "  Cleanup complete!" << std::endl;
    LOG_CORE_INFO("Test cleanup complete");

    WaitForInput();

    // ========================================================================
    // Test 5: Threading Primitives
    // ========================================================================
    LOG_CORE_INFO("=== Test 5: Threading Primitives ===");
    std::cout << "\n--- Threading Primitives ---" << std::endl;

    std::cout << "Hardware concurrency: " << Thread::GetHardwareConcurrency() << " threads" << std::endl;
    LOG_CORE_INFO("Hardware concurrency: {} threads", Thread::GetHardwareConcurrency());

    // Test Mutex and LockGuard
    std::cout << "\nTesting Mutex and LockGuard..." << std::endl;
    Mutex mutex;
    int counter = 0;
    std::vector<Thread> threads;

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&mutex, &counter, i]() {
            for (int j = 0; j < 100; ++j) {
                LockGuard lock(mutex);
                ++counter;
            }
            LOG_CORE_DEBUG("Thread {} finished", i);
        });
    }

    for (auto& thread : threads) {
        thread.Join();
    }

    std::cout << "  Counter value (should be 500): " << counter << std::endl;
    LOG_CORE_INFO("Mutex test - counter value: {}", counter);

    // Test AtomicCounter
    std::cout << "\nTesting AtomicCounter..." << std::endl;
    AtomicCounter atomicCounter(0);
    threads.clear();

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&atomicCounter]() {
            for (int j = 0; j < 100; ++j) {
                atomicCounter.Increment();
            }
        });
    }

    for (auto& thread : threads) {
        thread.Join();
    }

    std::cout << "  Atomic counter value (should be 500): " << atomicCounter.Get() << std::endl;
    LOG_CORE_INFO("Atomic counter test - value: {}", atomicCounter.Get());

    // Test ThreadPool
    std::cout << "\nTesting ThreadPool..." << std::endl;
    {
        ThreadPool pool(4);
        AtomicCounter taskCounter(0);

        for (int i = 0; i < 20; ++i) {
            pool.Enqueue([&taskCounter, i]() {
                Timing::SleepMilliseconds(10);
                taskCounter.Increment();
                LOG_CORE_TRACE("Task {} completed", i);
            });
        }

        pool.WaitForCompletion();
        std::cout << "  Completed " << taskCounter.Get() << " tasks" << std::endl;
        LOG_CORE_INFO("ThreadPool completed {} tasks", taskCounter.Get());
    }

    WaitForInput();

    // ========================================================================
    // Test 6: Virtual File System
    // ========================================================================
    LOG_CORE_INFO("=== Test 6: Virtual File System ===");
    std::cout << "\n--- Virtual File System ---" << std::endl;

    auto& vfs = VirtualFileSystem::GetInstance();

    // Get current directory
    auto cwdResult = FileSystem::GetCurrentDirectory();
    std::string cwd = cwdResult.success ? cwdResult.data : ".";

    // Initialize VFS
    std::cout << "Initializing VFS..." << std::endl;
    bool vfsInit = vfs.Initialize(cwd);
    if (vfsInit) {
        std::cout << "  VFS initialized successfully!" << std::endl;
        LOG_CORE_INFO("VFS initialized");
    } else {
        std::cout << "  VFS initialization failed!" << std::endl;
        LOG_CORE_ERROR("VFS initialization failed");
    }

    // List mount points
    std::cout << "\nMount points:" << std::endl;
    auto mountPoints = vfs.GetMountPoints();
    for (const auto& mp : mountPoints) {
        std::string physicalPath = vfs.GetMountPath(mp);
        std::cout << "  " << mp << " -> " << physicalPath << std::endl;
        LOG_CORE_INFO("Mount: {} -> {}", mp, physicalPath);
    }

    // Test VFS operations
    std::cout << "\nTesting VFS file operations..." << std::endl;

    // Write to user:// directory
    std::string userFile = "user://test_vfs.txt";
    std::string vfsContent = "Hello from VFS!\nTesting virtual file system.\n";

    std::cout << "Writing to: " << userFile << std::endl;
    auto vfsWriteResult = vfs.WriteFile(userFile, vfsContent);
    if (vfsWriteResult.success) {
        std::cout << "  Write success!" << std::endl;
        LOG_CORE_INFO("VFS write successful: {}", userFile);

        // Resolve path
        auto resolveResult = vfs.ResolvePath(userFile);
        if (resolveResult.success) {
            std::cout << "  Resolves to: " << resolveResult.data << std::endl;
            LOG_CORE_INFO("Resolved {} to {}", userFile, resolveResult.data);
        }
    } else {
        std::cout << "  Write failed: " << vfsWriteResult.error << std::endl;
        LOG_CORE_ERROR("VFS write failed: {}", vfsWriteResult.error);
    }

    // Read from VFS
    std::cout << "\nReading from: " << userFile << std::endl;
    auto vfsReadResult = vfs.ReadFile(userFile);
    if (vfsReadResult.success) {
        std::cout << "  Read success! Content:" << std::endl;
        std::cout << "  ---" << std::endl;
        std::cout << "  " << vfsReadResult.data;
        std::cout << "  ---" << std::endl;
        LOG_CORE_INFO("VFS read successful");
    } else {
        std::cout << "  Read failed: " << vfsReadResult.error << std::endl;
        LOG_CORE_ERROR("VFS read failed: {}", vfsReadResult.error);
    }

    // Test read-only violation (try to write to app://)
    std::cout << "\nTesting read-only protection (writing to app://)..." << std::endl;
    auto roTest = vfs.WriteFile("app://test.txt", "This should fail");
    if (!roTest.success) {
        std::cout << "  Correctly blocked: " << roTest.error << std::endl;
        LOG_CORE_INFO("Read-only protection working correctly");
    } else {
        std::cout << "  Warning: Write succeeded when it should have been blocked!" << std::endl;
        LOG_CORE_WARN("Read-only protection may not be working");
    }

    // Cleanup VFS test file
    std::cout << "\nCleaning up VFS test file..." << std::endl;
    vfs.DeleteFile(userFile);
    vfs.Shutdown();
    std::cout << "  VFS cleanup complete!" << std::endl;

    LOG_CORE_INFO("=== All Platform Tests Complete ===");
    std::cout << "\n=== All Platform Tests Complete ===" << std::endl;
    std::cout << "Check lupine_platform.log for detailed output." << std::endl;
    std::cout << std::endl;

    lupine::Logger::Flush();
    lupine::Logger::Shutdown();
}
