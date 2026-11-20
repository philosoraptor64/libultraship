#pragma once

#undef _DLL

#include <string>
#include <stdint.h>
#include <mutex>
#include <vector>
#include <string>

#include "zip.h"

#include "resource/File.h"
#include "resource/Resource.h"
#include "resource/archive/Archive.h"

namespace Ship {
struct File;

class O2rArchive final : virtual public Archive {
  public:
    O2rArchive(const std::string& archivePath);
    ~O2rArchive();

    bool Open();
    bool Close();
    bool WriteFile(const std::string& filename, const std::vector<uint8_t>& data);

    std::shared_ptr<File> LoadFile(const std::string& filePath);
    std::shared_ptr<File> LoadFile(uint64_t hash);

  private:
    // libzip handles are not thread-safe: the game thread, the audio thread and LUS's loader threads each read
    // through a handle of their own, taken from this pool (as upstream libultraship does)
    zip_t* GetZipHandle();
    void ReleaseZipHandle(zip_t* handle);
    void ClosePool();

    zip_t* mZipArchive = nullptr;
    std::mutex mPoolMutex;
    std::vector<zip_t*> mZipArchivePool;
};
} // namespace Ship
