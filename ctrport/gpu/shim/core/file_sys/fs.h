// SPDX-License-Identifier: GPL-2.0-or-later
// ctrport: equeue.cpp reserves a file handle per kernel queue. ctrport owns the
// guest file table, so queues get numbers from a separate range that never
// collides with ctrport file descriptors.
#pragma once
#include <map>
#include <memory>
#include <mutex>
#include "common/types.h"

namespace Core::FileSys {
enum class FileType { Regular, Directory, Device, Equeue };
struct File {
    FileType type = FileType::Regular;
};
class HandleTable {
public:
    s32 CreateHandle() {
        std::scoped_lock lock{mutex};
        const s32 handle = next++;
        files[handle] = std::make_unique<File>();
        return handle;
    }
    void DeleteHandle(s32 handle) {
        std::scoped_lock lock{mutex};
        files.erase(handle);
    }
    File* GetFile(s32 handle) {
        std::scoped_lock lock{mutex};
        const auto it = files.find(handle);
        return it == files.end() ? nullptr : it->second.get();
    }

private:
    std::mutex mutex;
    s32 next = 0x40000;
    std::map<s32, std::unique_ptr<File>> files;
};
} // namespace Core::FileSys
